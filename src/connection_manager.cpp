#include "netkit/connection_manager.h"
#include "netkit/utils.h"
#include "internal_server.h"
#include "internal_client.h"
#include "state.h"
#include <cstring>
#include <unistd.h>
using namespace std;

#include "threadkit/threadpool.h"
using namespace threadkit;

#define REQ_BUF_EXPAND_SIZE 1024

namespace netkit {

// client's refcount was increased before calling this function
void ConnectionManager::ProcessWriting(int64_t res, void* tag) {
    auto response = static_cast<Response*>(tag);
    auto client = response->client;
    int err = 0;

    if (!client->current_sending_res) {
        client->current_sending_res = response;
        err = m_wr_nq.SendAsync(client->fd_for_writing, qbuf_data(&response->data),
                                qbuf_size(&response->data), tag);
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
    if (client->bytes_sent < qbuf_size(&response->data)) {
        err = m_wr_nq.SendAsync(
            client->fd_for_writing,
            (const char*)qbuf_data(&response->data) + client->bytes_sent,
            qbuf_size(&response->data) - client->bytes_sent, tag);
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

    err = m_wr_nq.SendAsync(client->fd_for_writing, qbuf_data(&response->data),
                            qbuf_size(&response->data), response);
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

int ConnectionManager::Init() {
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

int ConnectionManager::DoAddClient(int64_t new_fd,
                                   const shared_ptr<RequestHandler>& handler) {
    auto client = new InternalClient(new_fd, handler);
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

int ConnectionManager::StartServer(const char* addr, uint16_t port,
                                   const shared_ptr<RequestHandlerFactory>& factory) {
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

void ConnectionManager::HandleAccept(int64_t new_fd, void* svr_ptr) {
    auto svr = static_cast<InternalServer*>(svr_ptr);

    if (new_fd <= 0) {
        logger_error(m_logger, "server down.");
        delete svr;
        return;
    }

    auto handler = shared_ptr<RequestHandler>(
        svr->factory->Create(),
        [f = svr->factory](RequestHandler* h) -> void {
            f->Destroy(h);
        });

    DoAddClient(new_fd, handler);
}

// client's refcount was increased before calling this function
void ConnectionManager::HandleInvalidRequest(void* client_ptr) {
    auto client = static_cast<InternalClient*>(client_ptr);
    logger_error(m_logger, "invalid request from [%s:%u].",
                 client->info.remote_addr.c_str(), client->info.remote_port);
    PutClient(client);
}

// client's refcount was increased before calling this function
void ConnectionManager::HandleMoreDataRequest(void* client_ptr, uint64_t expand_size) {
    auto client = static_cast<InternalClient*>(client_ptr);
    auto req = &client->req;
    auto err = req->Reserve(req->GetSize() + expand_size);
    if (err) {
        logger_error(m_logger, "allocate buffer for request failed: [%s].",
                     strerror(-err));
        goto errout;
    }

    err = m_new_rd_nq.RecvAsync(client->fd_for_reading,
                                (char*)req->GetData() + req->GetSize(),
                                expand_size, static_cast<State*>(client));
    if (err) {
        logger_error(m_logger, "about to recv data failed: [%s].", strerror(-err));
        goto errout;
    }

    return;

errout:
    PutClient(client);
}

// client's refcount was increased before calling this function
void ConnectionManager::HandleClientRequest(void* client_ptr) {
    auto client = static_cast<InternalClient*>(client_ptr);

again:
    uint64_t req_bytes = 0;
    auto req_stat = client->handler->Check(client->req, &req_bytes);

    if (req_stat == ReqStat::INVALID) {
        HandleInvalidRequest(client);
        return;
    }

    if (req_stat == ReqStat::MORE_DATA) {
        HandleMoreDataRequest(client, REQ_BUF_EXPAND_SIZE);
        return;
    }

    if (req_stat == ReqStat::MORE_DATA_WITH_SIZE) {
        client->bytes_needed = req_bytes;
        HandleMoreDataRequest(client, req_bytes);
        return;
    }

    // valid request

    client->bytes_needed = 0;

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
        HandleMoreDataRequest(client, REQ_BUF_EXPAND_SIZE);
        return;
    }

    goto again;

errout:
    PutClient(client);
}

// client's refcount was increased before calling this function
void ConnectionManager::HandleClientReading(int64_t res, void* client_ptr) {
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
    if (client->bytes_needed > 0) {
        client->bytes_needed -= res;
        if (client->bytes_needed > 0) {
            auto err = m_new_rd_nq.RecvAsync(
                client->fd_for_reading,
                client->req.GetData() + client->req.GetSize(),
                client->bytes_needed, static_cast<State*>(client));
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

// client's refcount was increased before calling *Async()
void ConnectionManager::ProcessNewAndReading(int64_t res, void* tag) {
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

    logger_error(m_logger, "unknown state [%u].", state->value);
}

void ConnectionManager::Loop() {
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
