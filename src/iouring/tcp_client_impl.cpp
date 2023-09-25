#include "netkit/iouring/tcp_client_impl.h"
#include "netkit/iouring/notification_queue_impl.h"
#include "../utils.h"
#include <unistd.h> // close()

namespace netkit { namespace iouring {

RetCode TcpClientImpl::Init(int fd, Logger*) {
    m_fd = fd;
    utils::GenConnectionInfo(fd, &m_info);
    return RC_OK;
}

RetCode TcpClientImpl::Init(const char* addr, uint16_t port, Logger* l) {
    int fd = utils::CreateTcpClientFd(addr, port, l);
    if (fd < 0) {
        logger_error(l, "create tcp client failed.");
        return RC_INTERNAL_NET_ERR;
    }

    return Init(fd, l);
}

void TcpClientImpl::Destroy() {
    if (m_fd > 0) {
        close(m_fd);
        m_fd = -1;
    }
}

RetCode TcpClientImpl::ReadAsync(void* buf, uint64_t sz, void* tag, NotificationQueue* nq) {
    auto q = (NotificationQueueImpl*)nq;
    return q->GenericAsync([buf, sz, tag, fd = m_fd](struct io_uring_sqe* sqe) -> void {
        io_uring_prep_read(sqe, fd, buf, sz, 0);
        io_uring_sqe_set_data(sqe, tag);
    });
}

RetCode TcpClientImpl::WriteAsync(const void* buf, uint64_t sz, void* tag, NotificationQueue* nq) {
    auto q = (NotificationQueueImpl*)nq;
    return q->GenericAsync([buf, sz, tag, fd = m_fd](struct io_uring_sqe* sqe) -> void {
        io_uring_prep_write(sqe, fd, buf, sz, 0);
        io_uring_sqe_set_data(sqe, tag);
    });
}

RetCode TcpClientImpl::ShutDownAsync(void* tag, NotificationQueue* nq) {
    auto q = (NotificationQueueImpl*)nq;
    return q->GenericAsync([tag, fd = m_fd](struct io_uring_sqe* sqe) -> void {
        io_uring_prep_shutdown(sqe, fd, SHUT_RDWR);
        io_uring_sqe_set_data(sqe, tag);
    });
}

}}
