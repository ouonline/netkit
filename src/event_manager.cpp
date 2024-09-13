#include "netkit/event_manager.h"
#include "netkit/utils.h"
#include "internal_server.h"
#include "internal_client.h"
#include "internal_timer_handler.h"
#include "state.h"
#include <cstring>
#include <unistd.h>
#include <sys/timerfd.h>
using namespace std;

#include "threadkit/threadpool.h"
using namespace threadkit;

#define REQ_BUF_EXPAND_SIZE 1024

namespace netkit {

int EventManager::AddTimer(const TimeVal& delay, const TimeVal& interval,
                           const function<void(int, uint64_t)>& handler) {
    int fd = timerfd_create(CLOCK_BOOTTIME, TFD_NONBLOCK | TFD_CLOEXEC);
    if (fd < 0) {
        logger_error(m_logger, "create timerfd failed: [%s].", strerror(errno));
        return -errno;
    }

    auto h = new InternalTimerHandler(fd, handler);
    if (!h) {
        logger_error(m_logger, "allocate InternalTimerHandler failed: [%s].",
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
        delete h;
        return -errno;
    }

    err = m_new_rd_nq.ReadAsync(fd, &h->nr_expiration, sizeof(uint64_t),
                                static_cast<State*>(h));
    if (err) {
        logger_error(m_logger, "about to read from timerfd failed: [%s].",
                     strerror(-err));
        delete h;
        return err;
    }

    return 0;
}

// client's refcount was increased before calling this function
void EventManager::ProcessWriting(int64_t res, void* tag) {
    auto response = static_cast<Response*>(tag);
    auto client = response->client;
    int err = 0;

    if (!client->current_sending_res) {
        client->current_sending_res = response;
        err = m_wr_nq.SendAsync(client->fd_for_writing, response->data.GetData(),
                                response->data.GetSize(), tag);
        if (err) {
            logger_error(m_logger, "send data failed: [%s].", strerror(-err));
            goto out;
        }
        return;
    }

    if (response != client->current_sending_res) {
        // `client->current_sending_res` is being sent. should wait in the queue.
        client->res_queue.Push(response);
        PutClient(client);
        return;
    }

    if (res < 0) {
        logger_error(m_logger, "send data to client [%s:%u] failed: [%s].",
                     client->info.remote_addr.c_str(),
                     client->info.remote_port, strerror(-res));
        err = res;
        goto out;
    }

    if (res == 0) {
        PutClient(client);
        return;
    }

    client->bytes_sent += res;
    if (client->bytes_sent < response->data.GetSize()) {
        err = m_wr_nq.SendAsync(
            client->fd_for_writing,
            response->data.GetData() + client->bytes_sent,
            response->data.GetSize() - client->bytes_sent, tag);
        if (!err) {
            return;
        }

        logger_error(m_logger, "about to send data failed: [%s].",
                     strerror(-err));
        goto out;
    }

    client->bytes_sent = 0;
    if (response->callback) {
        response->callback(0);
    }
    delete response;

    response = static_cast<Response*>(client->res_queue.PopNode());
    client->current_sending_res = response;
    if (!response) {
        PutClient(client);
        return;
    }

    err = m_wr_nq.SendAsync(client->fd_for_writing, response->data.GetData(),
                            response->data.GetSize(), response);
    if (!err) {
        return;
    }

    logger_error(m_logger, "send data failed: [%s].", strerror(-err));

out:
    if (response->callback) {
        response->callback(err);
    }
    PutClient(client);
}

int EventManager::Init() {
    auto ok = m_thread_pool.Init();
    if (!ok) {
        logger_error(m_logger, "init threadpool failed: [%s].", strerror(ENOMEM));
        return -ENOMEM;
    }

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

    m_signal_nq_list = make_unique<NotificationQueueImpl[]>(m_thread_pool.GetThreadNum());
    for (uint32_t i = 0; i < m_thread_pool.GetThreadNum(); ++i) {
        auto err = InitNq(&m_signal_nq_list[i], m_logger);
        if (err) {
            logger_error(m_logger, "init signal notification queue failed: [%s].",
                         strerror(-err));
            return err;
        }
    }

    err = InitNq(&m_new_rd_nq, m_logger);
    if (err) {
        logger_error(m_logger, "init new and recving notification queue failed: [%s].",
                     strerror(-err));
        return err;
    }

    return 0;
}

int EventManager::DoAddClient(int64_t new_fd,
                              const shared_ptr<Handler>& handler) {
    auto client = new InternalClient(new_fd, handler);
    if (!client) {
        logger_error(m_logger, "create InternalClient failed: [%s].",
                     strerror(ENOMEM));
        close(new_fd);
        return -ENOMEM;
    }

    {
        Sender sender(client, &m_wr_nq, &m_new_rd_nq);
        handler->OnConnected(&sender);
    }

    auto err = client->req.Reserve(REQ_BUF_EXPAND_SIZE);
    if (err) {
        logger_error(m_logger, "reserve [%lu] bytes for request failed: [%s].",
                     REQ_BUF_EXPAND_SIZE, strerror(-err));
        delete client;
        return err;
    }

    client->value = State::CLIENT_READ_REQ;
    GetClient(client);
    err = m_new_rd_nq.RecvAsync(client->fd_for_reading, client->req.GetData(),
                                REQ_BUF_EXPAND_SIZE, static_cast<State*>(client));
    if (err) {
        logger_error(m_logger, "about to recv data failed: [%s].", strerror(-err));
        PutClient(client);
    }
    return err;
}

int EventManager::AddServer(const char* addr, uint16_t port,
                            const shared_ptr<HandlerFactory>& factory) {
    int fd = utils::CreateTcpServerFd(addr, port, m_logger);
    if (fd < 0) {
        logger_error(m_logger, "create server for [%s:%u] failed: [%s].",
                     addr, port, strerror(fd));
        return fd;
    }

    auto svr = new InternalServer(fd, factory);
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
        delete svr;
        return err;
    }

    return 0;
}

int EventManager::AddClient(const char* addr, uint16_t port,
                            const shared_ptr<Handler>& h) {
    int fd = utils::CreateTcpClientFd(addr, port, m_logger);
    if (fd < 0) {
        logger_error(m_logger, "connect to [%s:%u] failed: [%s].", addr, port,
                     strerror(fd));
        return fd;
    }

    return DoAddClient(fd, h);
}

class ProcessReqTask final : public ThreadTask {
public:
    ProcessReqTask(Buffer&& req, InternalClient* c, NotificationQueueImpl* wr_nq,
                   NotificationQueueImpl* signal_nq_list)
        : m_req(std::move(req)), m_client(c), m_wr_nq(wr_nq)
        , m_signal_nq_list(signal_nq_list) {
        GetClient(c);
    }
    ~ProcessReqTask() {
        PutClient(m_client);
    }

    ThreadTask* Run(uint32_t idx) override {
        Sender sender(m_client, m_wr_nq, &m_signal_nq_list[idx]);
        m_client->handler->Process(std::move(m_req), &sender);
        return nullptr;
    }

private:
    Buffer m_req;
    InternalClient* m_client;
    NotificationQueueImpl* m_wr_nq;
    NotificationQueueImpl* m_signal_nq_list;
};

static void TaskDeleter(ThreadTask* t) {
    delete t;
}

void EventManager::HandleAccept(int64_t new_fd, void* svr_ptr) {
    auto svr = static_cast<InternalServer*>(svr_ptr);

    if (new_fd <= 0) {
        logger_error(m_logger, "server down.");
        delete svr;
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

// client's refcount was increased before calling this function
void EventManager::HandleClientRequest(void* client_ptr) {
    auto client = static_cast<InternalClient*>(client_ptr);

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

    // valid request

    client->bytes_left = 0;

    Buffer req_to_be_processed;
    if (req_bytes < client->req.GetSize()) {
        auto err = req_to_be_processed.Assign(client->req.GetData() + req_bytes,
                                              client->req.GetSize() - req_bytes);
        if (err) {
            logger_error(m_logger, "move request data failed: [%s].", strerror(-err));
            goto errout;
        }
        client->req.Resize(req_bytes);
    }
    std::swap(req_to_be_processed, client->req);

    auto task = new ProcessReqTask(std::move(req_to_be_processed), client,
                                   &m_wr_nq, m_signal_nq_list.get());
    if (!task) {
        logger_error(m_logger, "create ProcessReqTask failed: [%s].",
                     strerror(ENOMEM));
        goto errout;
    }

    task->deleter = TaskDeleter;
    m_thread_pool.AddTask(task);

    if (client->req.IsEmpty()) {
        HandleMoreDataRequest(client, 0);
        return;
    }

    goto again;

errout:
    PutClient(client);
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

    HandleClientRequest(client);
}

void EventManager::HandleTimerEvent(int64_t res, void* h) {
    auto handler = static_cast<InternalTimerHandler*>(h);

    if (res < 0) {
        logger_error(m_logger, "get timer event failed: [%s].", strerror(-res));
        handler->handler(res, 0);
        delete handler;
        return;
    }

    handler->handler(0, handler->nr_expiration);
    handler->nr_expiration = 0;

    int err = m_new_rd_nq.ReadAsync(handler->fd, &handler->nr_expiration,
                                    sizeof(uint64_t), static_cast<State*>(handler));
    if (err) {
        logger_error(m_logger, "about to read from timerfd failed: [%s].",
                     strerror(-err));
        handler->handler(err, 0);
        delete handler;
    }
}

// client's refcount was increased before calling *Async()
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

    if (state->value == State::TIMER) {
        auto handler = static_cast<InternalTimerHandler*>(state);
        HandleTimerEvent(res, handler);
        return;
    }

    logger_error(m_logger, "unknown state [%u].", state->value);
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
