#include "misc.h"
#include "netkit/tcp_client.h"
#include <string.h> // strerror()
using namespace std;

#define REQ_BUF_EXPAND_SIZE 1024

namespace netkit {

void TcpClient::DeleteSelf() {
    if (m_conn) {
        bool connected = (m_conn->fd >= 0);
        m_conn->ShutDown();
        if (connected) {
            OnDisconnected();
        }
    }
    delete this;
}

int TcpClient::Start(NotificationQueue* nq) {
    int err = m_buf.Reserve(REQ_BUF_EXPAND_SIZE);
    if (err) {
        logger_error(m_conn->logger,
                     "reserve [%lu] bytes for request failed: [%s].",
                     REQ_BUF_EXPAND_SIZE, strerror(-err));
        return err;
    }

    SendContext ctx(m_conn, nq);
    err = OnConnected(&ctx);
    if (err) {
        logger_error(m_conn->logger, "client OnConnected failed: [%s].",
                     strerror(-err));
        return err;
    }

    do {
        err = nq->ReadAsync(m_conn->fd, m_buf.data(), REQ_BUF_EXPAND_SIZE,
                            static_cast<EventHandler*>(this));
    } while (ShouldRetry(err));
    if (err) {
        logger_error(m_conn->logger, "about to recv data failed: [%s].",
                     strerror(-err));
        return err;
    }

    return 0;
}

bool TcpClient::HandleInvalidRequest() {
    const EndpointInfo& info = m_conn->GetEndpointInfo();
    logger_error(m_conn->logger, "invalid request from [%s:%u].",
                 info.remote_addr.c_str(), info.remote_port);
    return false;
}

bool TcpClient::HandleMoreDataRequest(uint32_t req_bytes,
                                      NotificationQueue* nq) {
    if (req_bytes == 0) {
        req_bytes = REQ_BUF_EXPAND_SIZE;
    } else {
        m_bytes_needed = req_bytes;
    }

    int err = m_buf.Reserve(m_buf.size() + req_bytes);
    if (err) {
        logger_error(m_conn->logger, "reserve [%lu] bytes failed: [%s].",
                     strerror(ENOMEM));
        return false;
    }

    do {
        err = nq->ReadAsync(m_conn->fd, m_buf.data() + m_buf.size(), req_bytes,
                            static_cast<EventHandler*>(this));
    } while (ShouldRetry(err));
    if (err) {
        logger_error(m_conn->logger, "launch read request failed: [%s].",
                     strerror(-err));
        return false;
    }

    return true;
}

int TcpClient::HandleValidRequest(uint32_t req_bytes, NotificationQueue* nq) {
    int err;
    Task* task;

    Buffer req;
    if (req_bytes < m_buf.size()) {
        err = req.Assign(m_buf.data() + req_bytes, m_buf.size() - req_bytes);
        if (err) {
            logger_error(m_conn->logger, "move request data failed: [%s].",
                         strerror(ENOMEM));
            return -1;
        }
        m_buf.Resize(req_bytes);
    }
    std::swap(req, m_buf);

    task = CreateTask();
    if (!task) {
        logger_error(m_conn->logger, "allocate Task failed: [%s].",
                     strerror(ENOMEM));
        return -1;
    }
    task->Init(move(req), m_conn);

    err = m_sched->Process(0, static_cast<EventHandler*>(task), nq);
    if (err) {
        logger_error(m_conn->logger,
                     "assign task to worker thread failed: [%s].",
                     strerror(-err));
        task->DeleteSelf();
        return -1;
    }

    if (m_buf.IsEmpty()) {
        bool ok = HandleMoreDataRequest(0, nq);
        if (!ok) {
            return -1;
        }
        return 0;
    }

    return 1;
}

bool TcpClient::Process(EventResult res, NotificationQueue* nq) {
    if (res.err) {
        if (ShouldRetry(-res.err)) {
            goto read_again;
        }

        logger_error(m_conn->logger, "read data failed: [%s].",
                     strerror(res.err));
        m_conn->ShutDown();
        return false;
    }
    if (res.val == 0) {
        return false;
    }

    m_buf.Resize(m_buf.size() + res.val);

read_again:
    if (m_bytes_needed > 0) {
        m_bytes_needed -= res.val;
        if (m_bytes_needed > 0) {
            int err =
                nq->ReadAsync(m_conn->fd, m_buf.data() + m_buf.size(),
                              m_bytes_needed, static_cast<EventHandler*>(this));
            if (err) {
                logger_error(m_conn->logger,
                             "launch read request failed: [%s].",
                             strerror(-err));
                return false;
            }
            return true;
        }
    }

    while (true) {
        uint32_t req_bytes = 0;
        auto req_stat = Check(m_buf, &req_bytes);

        if (req_stat == ReqStat::INVALID) {
            return HandleInvalidRequest();
        }

        if (req_stat == ReqStat::MORE_DATA) {
            return HandleMoreDataRequest(req_bytes, nq);
        }

        int rc = HandleValidRequest(req_bytes, nq);
        if (rc == -1) {
            return false;
        }
        if (rc == 0) {
            return true;
        }
    }

    return false; // unreachable
}

}
