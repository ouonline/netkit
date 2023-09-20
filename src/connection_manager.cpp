#include "netkit/connection_manager.h"
#include "read_write_request.h"
#include "shutdown_request.h"
#include "utils.h"
#include <cstring> // strerror()
using namespace std;

namespace netkit {

RetCode ConnectionManager::Init() {
    int err = io_uring_queue_init(64, &m_ring, 0);
    if (err) {
        logger_error(m_logger, "io_uring_queue_init failed: [%s].", strerror(err));
        return RC_INTERNAL_NET_ERR;
    }
    return RC_OK;
}

ConnectionManager::~ConnectionManager() {
    io_uring_queue_exit(&m_ring);
}

struct AcceptRequest final : public Request {
    int fd;
    function<void(const shared_ptr<Connection>&)> callback;
};

static RetCode AddAcceptRequest(struct io_uring* ring, AcceptRequest* acc_req, Logger* logger) {
    auto sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        io_uring_submit(ring);
        sqe = io_uring_get_sqe(ring);
        if (!sqe) {
            logger_error(logger, "io_uring_get_sqe failed.");
            return RC_INTERNAL_NET_ERR;
        }
    }

    io_uring_prep_multishot_accept(sqe, acc_req->fd, nullptr, nullptr, 0);
    io_uring_sqe_set_data(sqe, static_cast<Request*>(acc_req));

    int ret = io_uring_submit(ring);
    if (ret <= 0) {
        logger_error(logger, "io_uring_submit failed: [%s].", strerror(-ret));
        return RC_INTERNAL_NET_ERR;
    }

    return RC_OK;
}

static AcceptRequest* CreateAcceptRequest(int fd, const function<void(const shared_ptr<Connection>&)>& cb) {
    auto acc_req = new AcceptRequest();
    if (acc_req) {
        acc_req->type = Request::ACCEPT;
        acc_req->fd = fd;
        acc_req->callback = cb;
    }
    return acc_req;
}

RetCode ConnectionManager::CreateTcpServer(const char* addr, uint16_t port,
                                           const function<void(const shared_ptr<Connection>&)>& cb) {
    int fd = utils::CreateTcpServerFd(addr, port, m_logger);
    if (fd < 0) {
        logger_error(m_logger, "create server failed.");
        return RC_INTERNAL_NET_ERR;
    }

    auto acc_req = CreateAcceptRequest(fd, cb);
    if (!acc_req) {
        logger_error(m_logger, "CreateAcceptRequest failed.");
        return RC_NOMEM;
    }

    auto rc = AddAcceptRequest(&m_ring, acc_req, m_logger);
    if (rc != RC_OK) {
        shutdown(fd, SHUT_RDWR);
        delete acc_req;
        return rc;
    }

    return RC_OK;
}

shared_ptr<Connection> ConnectionManager::CreateTcpClient(const char* addr, uint16_t port) {
    int fd = utils::CreateTcpClientFd(addr, port, m_logger);
    if (fd < 0) {
        logger_error(m_logger, "create tcp client failed.");
        return shared_ptr<Connection>();
    }

    return make_shared<Connection>(fd, &m_ring, m_logger);
}

RetCode ConnectionManager::Run() {
    while (true) {
        struct io_uring_cqe* cqe = nullptr;
        Request* req;

        int ret = io_uring_wait_cqe(&m_ring, &cqe);
        if (ret < 0) {
            logger_error(m_logger, "wait cqe failed: [%s].", strerror(-ret));
            goto next;
        }
        if (cqe->res < 0) {
            logger_error(m_logger, "async request failed: [%s].", strerror(-cqe->res));
            goto next;
        }

        req = static_cast<Request*>(io_uring_cqe_get_data(cqe));
        switch (req->type) {
            case Request::ACCEPT: {
                auto acc_req = static_cast<AcceptRequest*>(req);
                auto conn = make_shared<Connection>(cqe->res, &m_ring, m_logger);
                acc_req->callback(conn);
                break;
            }
            case Request::READ: case Request::WRITE: {
                auto rw_req = static_cast<ReadWriteRequest*>(req);
                if (rw_req->callback) {
                    rw_req->callback(cqe->res);
                }
                delete rw_req;
                break;
            }
            case Request::SHUTDOWN: {
                auto sd_req = static_cast<ShutDownRequest*>(req);
                if (sd_req->callback) {
                    sd_req->callback();
                }
                delete sd_req;
                break;
            }
            default:
                logger_error(m_logger, "unknown request type [%u].", req->type);
                break;
        }

    next:
        io_uring_cqe_seen(&m_ring, cqe);
    }

    return RC_OK;
}

}
