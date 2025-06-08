#include "netkit/event_manager.h"
#include "internal_server.h"
#include "internal_client.h"
#include "internal_timer.h"
#include "internal_utils.h"
#include <cstring>
#include <assert.h>
using namespace std;

#include "threadkit/event_count.h"
using namespace threadkit;

#define REQ_BUF_EXPAND_SIZE 1024

namespace netkit {

void EventManager::Destroy() {
    if (m_worker_nq_list.empty()) {
        return;
    }

    int err = utils::InitThreadLocalNq(m_logger);
    if (err) {
        logger_error(m_logger, "InitThreadLocalNq failed: [%s].", strerror(-err));
        return;
    }

    auto nq = utils::GetThreadLocalNq();
    for (auto it = m_worker_nq_list.begin(); it != m_worker_nq_list.end(); ++it) {
        nq->NotifyAsync(*it, 0, nullptr);
    }
    m_worker_nq_list.clear();

    nq->NotifyAsync(m_wr_nq, 0, nullptr);

    for (auto it = m_worker_thread_list.begin(); it != m_worker_thread_list.end(); ++it) {
        it->join();
    }
    m_writing_thread.join();

    nq->NotifyAsync(&m_new_rd_nq, 0, nullptr);
}

// client's refcount was increased before calling this function
static void ProcessWriting(NotificationQueueImpl* wr_nq, int64_t res, void* tag,
                           Logger* logger) {
    auto session = static_cast<Session*>(tag);
    auto client = session->client;

    if (!client->current_sending) {
        client->current_sending = session;
        goto send_data;
    }

    if (session != client->current_sending) {
        // `client->current_sending` is being sent. should wait in the queue.
        client->send_queue.push(session);
        goto end;
    }

    if (res < 0) {
        const ConnectionInfo& info = client->conn.info();
        logger_error(logger, "send data to client [%s:%u] failed: [%s].",
                     info.remote_addr.c_str(), info.remote_port, strerror(-res));
        goto out;
    }

    if (res == 0) {
        // client disconnected
        goto out;
    }

    client->bytes_sent += res;
    if (client->bytes_sent == session->data.size()) {
        client->bytes_sent = 0;

        if (session->sent_callback) {
            session->sent_callback(0);
        }
        DestroySession(session);

        if (client->send_queue.empty()) {
            client->current_sending = nullptr;
            goto end;
        }

        session = client->send_queue.front();
        client->current_sending = session;
        client->send_queue.pop();
    }

send_data:
    res = wr_nq->SendAsync(client->fd_for_writing,
                           session->data.data() + client->bytes_sent,
                           session->data.size() - client->bytes_sent,
                           session);
    if (res == 0) {
        return;
    }

    logger_error(logger, "send data failed: [%s].", strerror(-res));

out:
    if (session->sent_callback) {
        session->sent_callback(res);
    }
    if (client->current_sending == session) {
        client->current_sending = nullptr;
    }
    DestroySession(session);
end:
    PutClient(client);
}

// client's refcount was increased before calling this function
static void WorkerProcessReq(Session* session) {
    auto client = session->client;
    client->handler->Process(std::move(session->data));
    DestroySession(session);
    PutClient(client);
}

// client's refcount was increased before calling this function
static void WorkerProcessTimer(InternalTimer* timer, int64_t res,
                               NotificationQueueImpl* nq,
                               NotificationQueueImpl* new_rd_nq,
                               Logger* logger) {
    Buffer buf;
    auto client = timer->client;

    if (res < 0) {
        logger_error(logger, "get timer failed: [%s].", strerror(-res));
        goto errout;
    }

    res = timer->callback(timer->nr_expiration);
    if (res) {
        logger_error(logger, "about to send buffer from timer failed: [%s].",
                     strerror(-res));
        goto errout;
    }

    timer->nr_expiration = 0;
    timer->value =State::TIMER_NEXT;
    res = nq->NotifyAsync(new_rd_nq, 0, timer);
    if (!res) {
        return;
    }

    logger_error(logger, "about to read from timerfd failed: [%s].",
                 strerror(-res));

errout:
    DestroyInternalTimer(timer);
    PutClient(client);
}

// client's refcount was increased before calling this function
static void WorkerThread(NotificationQueueImpl** worker_nq_pptr,
                         NotificationQueueImpl* new_rd_nq,
                         atomic<uint32_t>* counter, uint32_t max_count,
                         EventCount* cond, int* retcode, Logger* logger) {
    *retcode = utils::InitThreadLocalNq(logger);
    if ((*retcode)) {
        logger_error(logger, "InitThreadLocalNq failed: [%s].",
                     strerror(-(*retcode)));
        auto prev = counter->fetch_add(1, std::memory_order_acquire);
        if (prev + 1 == max_count) {
            cond->NotifyOne();
        }
        return;
    }

    auto nq = utils::GetThreadLocalNq();
    *worker_nq_pptr = nq; // saved in EventManager
    auto prev = counter->fetch_add(1, std::memory_order_acquire);
    if (prev + 1 == max_count) {
        cond->NotifyOne();
    }

    while (true) {
        int64_t res = 0;
        void* tag = nullptr;
        int err = nq->Next(&res, &tag, nullptr);
        if (err) {
            logger_error(logger, "get event failed: [%s].", strerror(-err));
            break;
        }
        if (!tag) {
            break;
        }

        auto state = static_cast<State*>(tag);
        if (state->value == State::WORKER_PROCESS_REQ) {
            auto session = static_cast<Session*>(state);
            WorkerProcessReq(session);
        } else if (state->value == State::TIMER_EXPIRED) {
            auto timer = static_cast<InternalTimer*>(state);
            WorkerProcessTimer(timer, res, nq, new_rd_nq, logger);
        } else {
            logger_fatal(logger, "unsupported state [%d].", state->value);
        }
    }
}

static void WritingThread(NotificationQueueImpl** wr_nq_pptr, atomic<uint32_t>* counter,
                          EventCount* cond, int* retcode, Logger* logger) {
    *retcode = utils::InitThreadLocalNq(logger);
    if (*retcode) {
        logger_error(logger, "init thread local notification queue failed: [%s].",
                     strerror(-(*retcode)));
        counter->store(1, std::memory_order_release);
        cond->NotifyOne();
        return;
    }

    auto wr_nq = utils::GetThreadLocalNq();
    *wr_nq_pptr = wr_nq; // saved in EventManager
    counter->store(1, std::memory_order_release);
    cond->NotifyOne();

    while (true) {
        int64_t res = 0;
        void* tag = nullptr;
        int err = wr_nq->Next(&res, &tag, nullptr);
        if (err) {
            logger_error(logger, "get event failed: [%s].", strerror(-err));
            break;
        }
        if (!tag) {
            break;
        }

        ProcessWriting(wr_nq, res, tag, logger);
    }
}

int EventManager::Init(const Options& options) {
    if (options.worker_num > 0) {
        m_worker_num = options.worker_num;
    } else {
        m_worker_num = std::max(std::thread::hardware_concurrency(), 2u) - 1;
    }

    int retcode;
    EventCount cond;

    atomic<uint32_t> finished_count = {0};
    m_writing_thread = thread(WritingThread, &m_wr_nq, &finished_count, &cond,
                              &retcode, m_logger);
    cond.Wait([&finished_count]() -> bool {
        return (finished_count.load(std::memory_order_acquire) > 0);
    });

    if (!m_wr_nq) {
        logger_error(m_logger, "init writing notification queue failed.",
                     strerror(-retcode));
        return retcode;
    }

    int err = InitNq(&m_new_rd_nq, m_logger);
    if (err) {
        logger_error(m_logger, "init new and recving notification queue failed: [%s].",
                     strerror(-err));
        return err;
    }

    m_worker_nq_list.resize(m_worker_num, nullptr);

    vector<int> retcode_list(m_worker_num, 0);
    finished_count.store(0, std::memory_order_release);

    m_worker_thread_list.reserve(m_worker_num);
    for (uint32_t idx = 0; idx < m_worker_num; ++idx) {
        m_worker_thread_list.emplace_back(
            thread(WorkerThread, &m_worker_nq_list[idx], &m_new_rd_nq,
                   &finished_count, m_worker_num, &cond, &retcode_list[idx], m_logger));
    }

    cond.Wait([&finished_count, worker_num = m_worker_num]() -> bool {
        return (finished_count.load(std::memory_order_acquire) == worker_num);
    });

    for (uint32_t i = 0; i < m_worker_num; ++i) {
        if (retcode_list[i] != 0) {
            logger_error(m_logger, "init thread [%u] failed: [%s].", i,
                         strerror(-retcode_list[i]));
            return retcode_list[i];
        }
    }

    return 0;
}

int EventManager::DoAddClient(int64_t new_fd, const shared_ptr<Handler>& handler) {
    auto client = CreateInternalClient(new_fd, handler, &m_new_rd_nq, m_wr_nq, m_logger);
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

    err = handler->OnConnected(&client->conn);
    if (err) {
        PutClient(client);
        return err;
    }

    client->value = State::CLIENT_READ_REQ;
    err = m_new_rd_nq.RecvAsync(client->fd_for_reading, client->req.data(),
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
                     addr, port, strerror(-fd));
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
                     strerror(-fd));
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
    const ConnectionInfo& info = client->conn.info();
    logger_error(m_logger, "invalid request from [%s:%u].",
                 info.remote_addr.c_str(), info.remote_port);
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

    auto err = req->Reserve(req->size() + req_bytes);
    if (err) {
        logger_error(m_logger, "allocate buffer for request failed: [%s].",
                     strerror(-err));
        goto errout;
    }

    err = m_new_rd_nq.RecvAsync(client->fd_for_reading,
                                (char*)req->data() + req->size(),
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
    assert(client->req.size() >= req_bytes);

    Buffer req_to_be_processed;
    if (req_bytes < client->req.size()) {
        err = req_to_be_processed.Assign(client->req.data() + req_bytes,
                                         client->req.size() - req_bytes);
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
    err = m_new_rd_nq.NotifyAsync(m_worker_nq_list[m_current_worker_idx], 0, session);
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
        const ConnectionInfo& info = client->conn.info();
        logger_error(m_logger, "recv data from client [%s:%u] failed: [%s].",
                     info.remote_addr.c_str(), info.remote_port, strerror(-res));
        PutClient(client);
        return;
    }

    if (res == 0) {
        PutClient(client);
        return;
    }

    // resize req to the real size after recving.
    // the real size is less than we reserved before recving.
    client->req.Resize(client->req.size() + res);

    // we already have a HandleClientRequest() before
    if (client->bytes_left > 0) {
        assert(client->bytes_left >= (uint64_t)res);
        client->bytes_left -= res;
        if (client->bytes_left > 0) {
            auto err = m_new_rd_nq.RecvAsync(
                client->fd_for_reading,
                client->req.data() + client->req.size(),
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

// client's refcount was increased before calling this function
void EventManager::HandleTimerExpired(int64_t res, void* state_ptr) {
    auto state = static_cast<State*>(state_ptr);
    auto timer = static_cast<InternalTimer*>(state);

    if (res < 0) {
        logger_error(m_logger, "get timer event failed: [%s].", strerror(-res));
        goto errout;
    }

    res = m_new_rd_nq.NotifyAsync(m_worker_nq_list[m_current_worker_idx],
                                  res, state_ptr);
    if (!res) {
        return;
    }

    logger_error(m_logger, "about to dispatch timer failed: [%s].",
                 strerror(-res));

errout:
    auto client = timer->client;
    timer->callback(res);
    DestroyInternalTimer(timer);
    PutClient(client);
}

// client's refcount was increased before calling this function
void EventManager::HandleTimerNext(void* state_ptr) {
    int err = 0;
    auto state = static_cast<State*>(state_ptr);
    auto timer = static_cast<InternalTimer*>(state);

    timer->value = State::TIMER_EXPIRED;
    err = m_new_rd_nq.ReadAsync(timer->fd, &timer->nr_expiration, sizeof(uint64_t),
                                static_cast<State*>(timer));
    if (!err) {
        return;
    }

    logger_error(m_logger, "about to read from timerfd failed: [%s].",
                 strerror(-err));
    auto client = timer->client;
    timer->callback(err);
    DestroyInternalTimer(timer);
    PutClient(client);
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
        if (!tag) {
            break;
        }

        ProcessNewAndReading(res, tag);
    }
}

}
