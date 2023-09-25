#include "netkit/epoll/tcp_server_impl.h"
#include "netkit/epoll/notification_queue_impl.h"
#include "../utils.h"
#include <sys/epoll.h>
#include <cstring> // strerror()
#include <sys/socket.h> // shutdown()
#include <unistd.h> // close()

namespace netkit { namespace epoll {

RetCode TcpServerImpl::Init(const char* addr, uint16_t port, Logger* l) {
    m_fd = utils::CreateTcpServerFd(addr, port, l);
    if (m_fd < 0) {
        logger_error(l, "create tcp server failed.");
        return RC_INTERNAL_NET_ERR;
    }

    auto rc = utils::SetNonBlocking(m_fd, l);
    if (rc != RC_OK) {
        logger_error(l, "SetNonBlocking failed.");
        return rc;
    }

    m_logger = l;

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
    this->tag = tag;

    auto q = (NotificationQueueImpl*)nq;
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = static_cast<EventHandler*>(this);
    if (epoll_ctl(q->m_epfd, EPOLL_CTL_ADD, m_fd, &ev) != 0) {
        logger_error(m_logger, "epoll add server fd failed: [%s].", strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }

    return RC_OK;
}

int64_t TcpServerImpl::In() {
    int fd = accept(m_fd, nullptr, nullptr);
    if (fd < 0) {
        return -errno;
    }
    return fd;
}

}}
