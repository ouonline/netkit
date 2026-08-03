#include "misc.h"
#include "netkit/tcp_client.h"
#include <string.h> // strerror()
using namespace std;

#define REQ_BUF_EXPAND_SIZE 1024

namespace netkit {

void TcpClient::DeleteSelf() {
    if (m_conn) {
        bool connected = (m_conn->fd >= 0);
        m_conn->ShutDown(m_logger);
        if (connected) {
            OnDisconnected();
        }
    }
    delete this;
}

int TcpClient::DoRead(void* buf, uint64_t sz, NotificationQueue* nq) {
loop:
    int err =
        nq->ReadAsync(m_conn->fd, buf, sz, static_cast<EventHandler*>(this));
    if (ShouldRetry(err)) {
        goto loop;
    }

    if (err) {
        logger_error(m_logger, "reading data failed: [%s].", strerror(-err));
        // fall through
    }

    return err;
}

int TcpClient::Start(NotificationQueue* nq) {
    int err = m_buf.Reserve(REQ_BUF_EXPAND_SIZE);
    if (err) {
        logger_error(m_logger, "reserve [%lu] bytes for request failed: [%s].",
                     REQ_BUF_EXPAND_SIZE, strerror(-err));
        return err;
    }

    SendContext ctx(m_conn, nq, m_logger);
    err = OnConnected(&ctx);
    if (err) {
        logger_error(m_logger, "client OnConnected failed: [%s].",
                     strerror(-err));
        return err;
    }

    return DoRead(m_buf.data(), REQ_BUF_EXPAND_SIZE, nq);
}

void TcpClient::HandleInvalidRequest() {
    const EndpointInfo& info = m_conn->GetEndpointInfo();
    logger_error(m_logger, "invalid request from [%s:%u].",
                 info.remote_addr.c_str(), info.remote_port);
}

int TcpClient::HandleMoreDataRequest(uint32_t req_bytes,
                                     NotificationQueue* nq) {
    if (req_bytes == 0) {
        req_bytes = REQ_BUF_EXPAND_SIZE;
    } else {
        m_bytes_needed = req_bytes;
    }

    int err = m_buf.Reserve(m_buf.size() + req_bytes);
    if (err) {
        logger_error(m_logger, "reserve [%lu] bytes failed: [%s].",
                     strerror(ENOMEM));
        return -ENOMEM;
    }

    return DoRead(m_buf.data() + m_buf.size(), req_bytes, nq);
}

int TcpClient::HandleValidRequest(uint32_t req_bytes, NotificationQueue* nq) {
    int err;

    Buffer req;
    if (req_bytes < m_buf.size()) {
        err = req.Assign(m_buf.data() + req_bytes, m_buf.size() - req_bytes);
        if (err) {
            logger_error(m_logger, "move request data failed: [%s].",
                         strerror(ENOMEM));
            return -1;
        }
        m_buf.Resize(req_bytes);
    }
    std::swap(req, m_buf);

    TaskPtr ptr = CreateTask();
    if (!ptr) {
        logger_error(m_logger, "allocate Task failed: [%s].", strerror(ENOMEM));
        return -1;
    }

    Task* task = ptr.release();
    task->Init(move(req), m_conn);

    err = m_sched->Schedule(0, static_cast<EventHandler*>(task), nq);
    if (err) {
        logger_error(m_logger, "assign task to worker thread failed: [%s].",
                     strerror(-err));
        task->DeleteSelf();
        return -1;
    }

    if (m_buf.IsEmpty()) {
        err = HandleMoreDataRequest(0, nq);
        if (err) {
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

        logger_error(m_logger, "read data failed: [%s].", strerror(res.err));
        m_conn->ShutDown(m_logger);
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
                logger_error(m_logger, "launch read request failed: [%s].",
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
            HandleInvalidRequest();
            return false;
        }

        if (req_stat == ReqStat::MORE_DATA) {
            return (HandleMoreDataRequest(req_bytes, nq) == 0);
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
