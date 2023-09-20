#include "netkit/connection.h"
#include "read_write_request.h"
#include "shutdown_request.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring> // strerror()
using namespace std;

namespace netkit {

Connection::Connection(int fd, struct io_uring* ring, Logger* logger)
    : m_fd(fd), m_ring(ring), m_logger(logger) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int ret = getpeername(fd, (struct sockaddr*)&addr, &len);
    if (ret == 0) {
        m_info.remote_addr = inet_ntoa(addr.sin_addr);
        m_info.remote_port = addr.sin_port;
    }

    len = sizeof(addr);
    ret = getsockname(fd, (struct sockaddr*)&addr, &len);
    if (ret == 0) {
        m_info.local_addr = inet_ntoa(addr.sin_addr);
        m_info.local_port = addr.sin_port;
    }
}

RetCode Connection::ReadAsync(void* buf, uint64_t sz, const function<void(uint64_t)>& cb) {
    auto sqe = io_uring_get_sqe(m_ring);
    if (!sqe) {
        io_uring_submit(m_ring);
        sqe = io_uring_get_sqe(m_ring);
        if (!sqe) {
            logger_error(m_logger, "io_uring_get_sqe failed.");
            return RC_INTERNAL_NET_ERR;
        }
    }

    auto rd_req = new ReadWriteRequest(cb, Request::READ);
    if (!rd_req) {
        return RC_NOMEM;
    }

    io_uring_prep_read(sqe, m_fd, buf, sz, 0);
    io_uring_sqe_set_data(sqe, rd_req);

    int ret = io_uring_submit(m_ring);
    if (ret <= 0) {
        logger_error(m_logger, "io_uring_submit failed: [%s].", strerror(-ret));
        delete rd_req;
        return RC_INTERNAL_NET_ERR;
    }

    return RC_OK;
}

RetCode Connection::WriteAsync(const void* buf, uint64_t sz, const function<void(uint64_t)>& cb) {
    auto sqe = io_uring_get_sqe(m_ring);
    if (!sqe) {
        io_uring_submit(m_ring);
        sqe = io_uring_get_sqe(m_ring);
        if (!sqe) {
            logger_error(m_logger, "io_uring_get_sqe failed.");
            return RC_INTERNAL_NET_ERR;
        }
    }

    auto wr_req = new ReadWriteRequest(cb, Request::WRITE);
    if (!wr_req) {
        return RC_NOMEM;
    }

    io_uring_prep_write(sqe, m_fd, buf, sz, 0);
    io_uring_sqe_set_data(sqe, wr_req);

    int ret = io_uring_submit(m_ring);
    if (ret <= 0) {
        logger_error(m_logger, "io_uring_submit failed: [%s].", strerror(-ret));
        delete wr_req;
        return RC_INTERNAL_NET_ERR;
    }

    return RC_OK;
}

RetCode Connection::ShutDownAsync(const function<void()>& cb) {
    auto sqe = io_uring_get_sqe(m_ring);
    if (!sqe) {
        io_uring_submit(m_ring);
        sqe = io_uring_get_sqe(m_ring);
        if (!sqe) {
            logger_error(m_logger, "io_uring_get_sqe failed.");
            return RC_INTERNAL_NET_ERR;
        }
    }

    auto sd_req = new ShutDownRequest(cb);
    if (!sd_req) {
        return RC_NOMEM;
    }

    io_uring_prep_shutdown(sqe, m_fd, SHUT_RDWR);
    io_uring_sqe_set_data(sqe, sd_req);

    int ret = io_uring_submit(m_ring);
    if (ret <= 0) {
        logger_error(m_logger, "io_uring_submit failed: [%s].", strerror(-ret));
        delete sd_req;
        return RC_INTERNAL_NET_ERR;
    }

    return RC_OK;
}

}
