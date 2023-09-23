#include "netkit/iouring/connection.h"
#include "iouring_utils.h"
#include <arpa/inet.h>

namespace netkit { namespace iouring {

Connection::~Connection() {
    if (m_fd > 0) {
        close(m_fd);
    }
}

RetCode Connection::Init(int fd, struct io_uring* ring, Logger* logger) {
    m_fd = fd;
    m_ring = ring;
    m_logger = logger;

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

    return RC_OK;
}

RetCode Connection::ReadAsync(void* buf, uint64_t sz, void* tag) {
    return GenericAsync(m_ring, m_logger, [buf, sz, tag, fd = m_fd](struct io_uring_sqe* sqe) -> void {
        io_uring_prep_read(sqe, fd, buf, sz, 0);
        io_uring_sqe_set_data(sqe, tag);
    });
}

RetCode Connection::WriteAsync(const void* buf, uint64_t sz, void* tag) {
    return GenericAsync(m_ring, m_logger, [buf, sz, tag, fd = m_fd](struct io_uring_sqe* sqe) -> void {
        io_uring_prep_write(sqe, fd, buf, sz, 0);
        io_uring_sqe_set_data(sqe, tag);
    });
}

RetCode Connection::ShutDownAsync(void* tag) {
    return GenericAsync(m_ring, m_logger, [tag, fd = m_fd](struct io_uring_sqe* sqe) -> void {
        io_uring_prep_shutdown(sqe, fd, SHUT_RDWR);
        io_uring_sqe_set_data(sqe, tag);
    });
}

}}
