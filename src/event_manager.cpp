#include "netkit/event_manager.h"
#include "netkit/utils.h"
#include "internal_server.h"
#include "internal_client.h"
#include "internal_timer.h"
#include "state.h"
#include <cstring>
#include <unistd.h>
#include <sys/timerfd.h>
#include <assert.h>
using namespace std;

#define REQ_BUF_EXPAND_SIZE 1024

namespace netkit {

int EventManager::AddTimer(const TimeVal& delay, const TimeVal& interval,
                           const function<void(int32_t)>& handler) {
    int fd = timerfd_create(CLOCK_BOOTTIME, TFD_NONBLOCK | TFD_CLOEXEC);
    if (fd < 0) {
        logger_error(m_logger, "create timerfd failed: [%s].", strerror(errno));
        return -errno;
    }

    auto timer = CreateInternalTimer(fd, handler);
    if (!timer) {
        logger_error(m_logger, "allocate InternalTimer failed: [%s].",
                     strerror(ENOMEM));
        close(fd);
        return -ENOMEM;
    }

    const struct itimerspec ts = {
        .it_interval = {
            .tv_sec = interval.tv_sec,
            .tv_nsec = interval.tv_usec * 1000,
        },
        .it_value = {
            .tv_sec = delay.tv_sec,
            .tv_nsec = delay.tv_usec * 1000,
        },
    };

    int err = timerfd_settime(fd, 0, &ts, nullptr);
    if (err) {
        logger_error(m_logger, "timerfd_settime failed: [%s].", strerror(errno));
        DestroyInternalTimer(timer);
        return -errno;
    }

    timer->value = State::TIMER_EXPIRED;
    err = m_new_rd_nq.ReadAsync(fd, &timer->nr_expiration, sizeof(uint64_t),
                                static_cast<State*>(timer));
    if (err) {
        logger_error(m_logger, "about to read from timerfd failed: [%s].",
                     strerror(-err));
        DestroyInternalTimer(timer);
        return err;
    }

    return fd;
}

// client's refcount was increased before calling this function
void EventManager::ProcessWriting(int64_t res, void* tag) {
    auto session = static_cast<Session*>(tag);
    auto client = session->client;

    if (!client->current_sending) {
        client->current_sending = session;
        goto send_data;
    }

    if (session != client->current_sending) {
        // `client->current_sending` is being sent. should wait in the queue.
        auto node = GetNodeFromSession(session);
        client->res_queue.Push(node);
        goto out;
    }

    if (res < 0) {
        logger_error(m_logger, "send data to client [%s:%u] failed: [%s].",
                     client->info.remote_addr.c_str(),
                     client->info.remote_port, strerror(-res));
        goto out;
    }

    if (res == 0) {
        // client disconnected
        goto out;
    }

    client->bytes_sent += res;
    if (client->bytes_sent == session->data.GetSize()) {
        client->bytes_sent = 0;
        DestroySession(session);

        auto node = client->res_queue.PopNode();
        if (!node) {
            client->current_sending = nullptr;
            goto out;
        }

        session = GetSessionFromNode(node);
        client->current_sending = session;
    }

send_data:
    res = m_wr_nq.SendAsync(client->fd_for_writing,
                            session->data.GetData() + client->bytes_sent,
                            session->data.GetSize() - client->bytes_sent,
                            session);
    if (!res) {
        return;
    }

    logger_error(m_logger, "send data failed: [%s].", strerror(-res));

out:
    PutClient(client);
}

// client's refcount was increased before calling this function
static void WorkerProcessReq(Session* session, NotificationQueueImpl* nq,
                             NotificationQueueImpl* wr_nq, Logger* logger) {
    int err = 0;
    auto client = session->client;

    Buffer res_data;
    client->handler->Process(std::move(session->data), &res_data);
    if (res_data.IsEmpty()) {
        goto end;
    }

    session->data = std::move(res_data);
    err = nq->NotifyAsync(wr_nq, 0, session);
    if (!err) {
        return;
    }

    logger_error(logger, "notify sending queue failed: [%s].", strerror(-err));

end:
    DestroySession(session);
    PutClient(client);
}

static void WorkerProcessTimer(InternalTimer* timer, int64_t res, NotificationQueueImpl* nq,
                               NotificationQueueImpl* new_rd_nq, Logger* logger) {
    if (res < 0) {
        logger_error(logger, "get timer failed: [%s].", strerror(-res));
        goto errout;
    }

    timer->handler(timer->nr_expiration);
    timer->nr_expiration = 0;

    timer->value =State::TIMER_NEXT;
    res = nq->NotifyAsync(new_rd_nq, 0, timer);
    if (!res) {
        return;
    }

    logger_error(logger, "about to read from timerfd failed: [%s].",
                 strerror(-res));

errout:
    timer->handler(res);
    DestroyInternalTimer(timer);
}

// client's refcount was increased before calling this function
static void WorkerThread(NotificationQueueImpl* nq, NotificationQueueImpl* new_rd_nq,
                         NotificationQueueImpl* wr_nq, Logger* logger) {
    while (true) {
        int64_t res = 0;
        void* tag = nullptr;
        auto err = nq->Next(&res, &tag, nullptr);
        if (err) {
            logger_error(logger, "get event failed: [%s].", strerror(-err));
            break;
        }

        auto state = static_cast<State*>(tag);
        if (state->value == State::WORKER_PROCESS_REQ) {
            auto session = static_cast<Session*>(state);
            WorkerProcessReq(session, nq, wr_nq, logger);
        } else if (state->value == State::TIMER_EXPIRED) {
            auto timer = static_cast<InternalTimer*>(state);
            WorkerProcessTimer(timer, res, nq, new_rd_nq, logger);
        } else {
            logger_fatal(logger, "unsupported state [%d].", state->value);
        }
    }
}

int EventManager::Init() {
    auto err = InitNq(&m_wr_nq, m_logger);
    if (err) {
        logger_error(m_logger, "init writing notification queue failed: [%s].",
                     strerror(-err));
        return err;
    }

    m_writing_thread = thread([this]() -> void {
        while (true) {
            int64_t res = 0;
            void* tag = nullptr;
            auto err = m_wr_nq.Next(&res, &tag, nullptr);
            if (err) {
                logger_error(m_logger, "get event failed: [%s].", strerror(-err));
                break;
            }

            ProcessWriting(res, tag);
        }
    });

    m_worker_num = std::max(std::thread::hardware_concurrency(), 2u) - 1;

    m_worker_nq_list = make_unique<NotificationQueueImpl[]>(m_worker_num);
    for (uint32_t i = 0; i < m_worker_num; ++i) {
        err = InitNq(&m_worker_nq_list[i], m_logger);
        if (err) {
            logger_error(m_logger, "init signal notification queue failed: [%s].",
                         strerror(-err));
            return err;
        }
    }

    m_worker_thread_list.reserve(m_worker_num);
    for (uint32_t i = 0; i < m_worker_num; ++i) {
        m_worker_thread_list.emplace_back(
            thread(WorkerThread, &m_worker_nq_list[i], &m_new_rd_nq, &m_wr_nq, m_logger));
    }

    err = InitNq(&m_new_rd_nq, m_logger);
    if (err) {
        logger_error(m_logger, "init new and recving notification queue failed: [%s].",
                     strerror(-err));
        return err;
    }

    return 0;
}

int EventManager::DoAddClient(int64_t new_fd, const shared_ptr<Handler>& handler) {
    auto client = CreateInternalClient(new_fd, handler);
    if (!client) {
        logger_error(m_logger, "create InternalClient failed: [%s].",
                     strerror(ENOMEM));
        close(new_fd);
        return -ENOMEM;
    }

    auto err = client->req.Reserve(REQ_BUF_EXPAND_SIZE);
    if (err) {
        logger_error(m_logger, "reserve [%lu] bytes for request failed: [%s].",
                     REQ_BUF_EXPAND_SIZE, strerror(-err));
        DestroyInternalClient(client);
        return err;
    }

    // retain the client for RecvAsync()
    GetClient(client);

    Buffer buf;
    handler->OnConnected(client->info, &buf);
    if (!buf.IsEmpty()) {
        auto session = CreateSession();
        if (!session) {
            logger_error(m_logger, "create Session failed: [%s].", strerror(ENOMEM));
            DestroyInternalClient(client); // ok
            return -ENOMEM;
        }

        GetClient(client);
        session->data = std::move(buf);
        session->client = client;
        err = m_new_rd_nq.NotifyAsync(&m_wr_nq, 0, session);
        if (err) {
            logger_error(m_logger, "about to send data failed: [%s].", strerror(-err));
            DestroySession(session);
            DestroyInternalClient(client); // ok
            return err;
        }
    }

    client->value = State::CLIENT_READ_REQ;
    err = m_new_rd_nq.RecvAsync(client->fd_for_reading, client->req.GetData(),
                                REQ_BUF_EXPAND_SIZE, static_cast<State*>(client));
    if (err) {
        logger_error(m_logger, "about to recv data failed: [%s].", strerror(-err));
        PutClient(client);
        return err;
    }

    return new_fd;
}

int EventManager::AddServer(const char* addr, uint16_t port,
                            const shared_ptr<HandlerFactory>& factory) {
    int fd = utils::CreateTcpServerFd(addr, port, m_logger);
    if (fd < 0) {
        logger_error(m_logger, "create server for [%s:%u] failed: [%s].",
                     addr, port, strerror(fd));
        return fd;
    }

    auto svr = CreateInternalServer(fd, factory);
    if (!svr) {
        close(fd);
        logger_error(m_logger, "create InternalServer for [%s:%u] failed: [%s].",
                     addr, port, strerror(ENOMEM));
        return -ENOMEM;
    }

    svr->value = State::SERVER_ACCEPT;
    auto err = m_new_rd_nq.MultiAcceptAsync(fd, static_cast<State*>(svr));
    if (err != 0) {
        logger_error(m_logger, "add server to notification queue failed: [%s].",
                     strerror(-err));
        DestroyInternalServer(svr);
        return err;
    }

    return fd;
}

int EventManager::AddClient(const char* addr, uint16_t port, const shared_ptr<Handler>& h) {
    int fd = utils::CreateTcpClientFd(addr, port, m_logger);
    if (fd < 0) {
        logger_error(m_logger, "connect to [%s:%u] failed: [%s].", addr, port,
                     strerror(fd));
        return fd;
    }

    return DoAddClient(fd, h);
}

void EventManager::HandleAccept(int64_t new_fd, void* svr_ptr) {
    auto svr = static_cast<InternalServer*>(svr_ptr);

    if (new_fd <= 0) {
        logger_error(m_logger, "server down: [%s].", strerror(-errno));
        DestroyInternalServer(svr);
        return;
    }

    auto handler = shared_ptr<Handler>(
        svr->factory->Create(),
        [f = svr->factory](Handler* h) -> void {
            f->Destroy(h);
        });

    DoAddClient(new_fd, handler);
}

// client's refcount was increased before calling this function
void EventManager::HandleInvalidRequest(void* client_ptr) {
    auto client = static_cast<InternalClient*>(client_ptr);
    logger_error(m_logger, "invalid request from [%s:%u].",
                 client->info.remote_addr.c_str(), client->info.remote_port);
    PutClient(client);
}

// client's refcount was increased before calling this function
void EventManager::HandleMoreDataRequest(void* client_ptr, uint64_t req_bytes) {
    auto client = static_cast<InternalClient*>(client_ptr);
    auto req = &client->req;

    assert(client->bytes_left == 0);

    if (req_bytes == 0) {
        req_bytes = REQ_BUF_EXPAND_SIZE;
    } else {
        client->bytes_left = req_bytes;
    }

    auto err = req->Reserve(req->GetSize() + req_bytes);
    if (err) {
        logger_error(m_logger, "allocate buffer for request failed: [%s].",
                     strerror(-err));
        goto errout;
    }

    err = m_new_rd_nq.RecvAsync(client->fd_for_reading,
                                (char*)req->GetData() + req->GetSize(),
                                req_bytes, static_cast<State*>(client));
    if (err) {
        logger_error(m_logger, "about to recv data failed: [%s].", strerror(-err));
        goto errout;
    }

    return;

errout:
    PutClient(client);
}

// returns true means to handle next request
// client's refcount was increased before calling this function
bool EventManager::HandleValidRequest(void* client_ptr, uint64_t req_bytes) {
    int err;
    Session* session;
    auto client = static_cast<InternalClient*>(client_ptr);

    assert(client->bytes_left == 0);
    assert(client->req.GetSize() >= req_bytes);

    Buffer req_to_be_processed;
    if (req_bytes < client->req.GetSize()) {
        err = req_to_be_processed.Assign(client->req.GetData() + req_bytes,
                                         client->req.GetSize() - req_bytes);
        if (err) {
            logger_error(m_logger, "move request data failed: [%s].", strerror(-err));
            goto errout;
        }
        client->req.Resize(req_bytes);
    }
    std::swap(req_to_be_processed, client->req);

    session = CreateSession();
    if (!session) {
        logger_error(m_logger, "allocate Session failed: [%s].", strerror(ENOMEM));
        goto errout;
    }

    session->value = State::WORKER_PROCESS_REQ;
    session->data = std::move(req_to_be_processed);
    session->client = client;

    GetClient(client);
    err = m_new_rd_nq.NotifyAsync(&m_worker_nq_list[m_current_worker_idx], 0, session);
    if (err) {
        logger_error(m_logger, "send request to worker thread failed: [%s].",
                     strerror(-err));
        DestroySession(session);
        PutClient(client);
        goto errout;
    }

    m_current_worker_idx = (m_current_worker_idx + 1) % m_worker_num;

    if (client->req.IsEmpty()) {
        HandleMoreDataRequest(client, 0);
        return false;
    }

    // don't PutClient(). should be left for the next round
    return true;

errout:
    PutClient(client);
    return false;
}

// client's refcount was increased before calling this function
void EventManager::HandleClientReading(int64_t res, void* client_ptr) {
    auto client = static_cast<InternalClient*>(client_ptr);

    if (res < 0) {
        logger_error(m_logger, "recv data from client [%s:%u] failed: [%s].",
                     client->info.remote_addr.c_str(),
                     client->info.remote_port, strerror(-res));
        PutClient(client);
        return;
    }

    if (res == 0) {
        PutClient(client);
        return;
    }

    // resize req to the real size after recving.
    // the real size is less than we reserved before recving.
    client->req.Resize(client->req.GetSize() + res);

    // we already have a HandleClientRequest() before
    if (client->bytes_left > 0) {
        assert(client->bytes_left >= (uint64_t)res);
        client->bytes_left -= res;
        if (client->bytes_left > 0) {
            auto err = m_new_rd_nq.RecvAsync(
                client->fd_for_reading,
                client->req.GetData() + client->req.GetSize(),
                client->bytes_left, static_cast<State*>(client));
            if (err) {
                logger_error(m_logger, "about to recv req failed: [%s].",
                             strerror(-err));
                PutClient(client);
            }
            return;
        }
    }

again:
    uint64_t req_bytes = 0;
    auto req_stat = client->handler->Check(client->req, &req_bytes);

    if (req_stat == ReqStat::INVALID) {
        HandleInvalidRequest(client);
        return;
    }

    if (req_stat == ReqStat::MORE_DATA) {
        HandleMoreDataRequest(client, req_bytes);
        return;
    }

    bool goto_next = HandleValidRequest(client, req_bytes);
    if (goto_next) {
        goto again;
    }
}

void EventManager::HandleTimerExpired(int64_t res, void* state_ptr) {
    auto state = static_cast<State*>(state_ptr);
    auto timer = static_cast<InternalTimer*>(state);

    if (res < 0) {
        logger_error(m_logger, "get timer event failed: [%s].", strerror(-res));
        goto errout;
    }

    res = m_new_rd_nq.NotifyAsync(&m_worker_nq_list[m_current_worker_idx],
                                  res, state_ptr);
    if (!res) {
        return;
    }

    logger_error(m_logger, "about to dispatch timer failed: [%s].",
                 strerror(-res));

errout:
    timer->handler(res);
    DestroyInternalTimer(timer);
}

void EventManager::HandleTimerNext(void* timer_ptr) {
    auto timer = static_cast<InternalTimer*>(timer_ptr);
    timer->value = State::TIMER_EXPIRED;
    int err = m_new_rd_nq.ReadAsync(timer->fd, &timer->nr_expiration, sizeof(uint64_t),
                                    static_cast<State*>(timer));
    if (err) {
        logger_error(m_logger, "about to read from timerfd failed: [%s].",
                     strerror(-err));
        DestroyInternalTimer(timer);
    }
}

void EventManager::ProcessNewAndReading(int64_t res, void* tag) {
    auto state = static_cast<State*>(tag);

    if (state->value == State::SERVER_ACCEPT) {
        auto svr = static_cast<InternalServer*>(state);
        HandleAccept(res, svr);
        return;
    }

    if (state->value == State::CLIENT_READ_REQ) {
        auto client = static_cast<InternalClient*>(state);
        HandleClientReading(res, client);
        return;
    }

    if (state->value == State::TIMER_EXPIRED) {
        auto timer = static_cast<InternalTimer*>(state);
        HandleTimerExpired(res, timer);
        return;
    }

    if (state->value == State::TIMER_NEXT) {
        auto timer = static_cast<InternalTimer*>(state);
        HandleTimerNext(timer);
        return;
    }

    logger_fatal(m_logger, "unknown state [%u]. res [%ld], tag [%p]",
                 state->value, res, tag);
}

void EventManager::Loop() {
    while (true) {
        int64_t res = 0;
        void* tag = nullptr;
        auto err = m_new_rd_nq.Next(&res, &tag, nullptr);
        if (err != 0) {
            logger_error(m_logger, "get event failed: [%s].", strerror(-err));
            break;
        }

        ProcessNewAndReading(res, tag);
    }
}

}
