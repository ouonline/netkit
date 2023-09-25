#include "netkit/iouring/tcp_server_impl.h"
#include "netkit/iouring/notification_queue_impl.h"
#include "../utils.h"
#include <sys/socket.h> // shutdown()
#include <unistd.h> // close()

namespace netkit { namespace iouring {

RetCode TcpServerImpl::Init(const char* addr, uint16_t port, Logger* l) {
    m_fd = utils::CreateTcpServerFd(addr, port, l);
    if (m_fd < 0) {
        logger_error(l, "create tcp server failed.");
        return RC_INTERNAL_NET_ERR;
    }

    return RC_OK;
}

void TcpServerImpl::Destroy() {
    if (m_fd) {
        shutdown(m_fd, SHUT_RDWR);
        close(m_fd);
        m_fd = -1;
    }
}

RetCode TcpServerImpl::MultiAcceptAsync(void* tag, NotificationQueue* nq) {
    auto q = (NotificationQueueImpl*)nq;
    return q->GenericAsync([fd = m_fd, tag](struct io_uring_sqe* sqe) -> void {
        io_uring_prep_multishot_accept(sqe, fd, nullptr, nullptr, 0);
        io_uring_sqe_set_data(sqe, tag);
    });
}

}}
