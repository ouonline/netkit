#include "netkit/epoll/tcp_client_impl.h"
#include "netkit/epoll/notification_queue_impl.h"
#include "../utils.h"
#include <sys/eventfd.h>
#include <unistd.h> // close()
#include <cstring> // strerror()
#include <sys/socket.h> // shutdown()
using namespace std;

namespace netkit { namespace epoll {

TcpClientImpl::TcpClientImpl(TcpClientImpl&& c) {
    m_fd = c.m_fd;
    m_written_fd = c.m_written_fd;
    m_shutdown_fd = c.m_shutdown_fd;
    m_read_handler = std::move(c.m_read_handler);
    m_written_handler = std::move(c.m_written_handler);
    m_shutdown_handler = std::move(c.m_shutdown_handler);
    m_fd_added = c.m_fd_added;
    m_written_fd_added = c.m_written_fd_added;
    m_shutdown_fd_added = c.m_shutdown_fd_added;
    m_logger = c.m_logger;

    m_fd = -1;
    m_written_fd = -1;
    m_shutdown_fd = -1;
    m_fd_added = false;
    m_written_fd_added = false;
    m_shutdown_fd_added = false;
    m_logger = nullptr;
}

RetCode TcpClientImpl::Init(int fd, Logger* logger) {
    m_fd = fd;
    m_logger = logger;

    auto rc = utils::SetNonBlocking(fd, logger);
    if (rc != RC_OK) {
        logger_error(logger, "SetNonBlocking of client fd failed.");
        return rc;
    }

    m_written_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (m_written_fd < 0) {
        logger_error(logger, "create write event fd failed: [%s].", strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }
    rc = utils::SetNonBlocking(m_written_fd, logger);
    if (rc != RC_OK) {
        logger_error(logger, "SetNonBlocking of write event fd failed.");
        return rc;
    }

    m_shutdown_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (m_shutdown_fd < 0) {
        logger_error(logger, "create shutdown event fd failed: [%s].", strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }
    rc = utils::SetNonBlocking(m_shutdown_fd, logger);
    if (rc != RC_OK) {
        logger_error(logger, "SetNonBlocking of shutdown event fd failed.");
        return rc;
    }

    m_read_handler.Init(m_fd);
    m_written_handler.Init(m_written_fd);
    m_shutdown_handler.Init(m_shutdown_fd);

    utils::GenConnectionInfo(m_fd, &m_info);

    return RC_OK;
}

RetCode TcpClientImpl::Init(const char* addr, uint16_t port, Logger* l) {
    int fd = utils::CreateTcpClientFd(addr, port, l);
    if (fd < 0) {
        logger_error(l, "create client failed.");
        return RC_INTERNAL_NET_ERR;
    }

    return Init(fd, l);
}

void TcpClientImpl::Destroy() {
    if (m_fd > 0) {
        shutdown(m_fd, SHUT_RDWR);
        close(m_fd);
        m_fd = -1;
    }
    if (m_written_fd > 0) {
        close(m_written_fd);
        m_written_fd = -1;
    }
    if (m_shutdown_fd > 0) {
        close(m_shutdown_fd);
        m_shutdown_fd = -1;
    }
}

RetCode TcpClientImpl::ReadAsync(void* buf, uint64_t sz, void* tag, NotificationQueue* nq) {
    m_read_handler.SetParameters(buf, sz, tag);

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    ev.data.ptr = static_cast<EventHandler*>(&m_read_handler);

    auto q = (NotificationQueueImpl*)nq;
    uint32_t op = m_fd_added ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    int err = epoll_ctl(q->m_epfd, op, m_fd, &ev);
    if (err) {
        logger_error(m_logger, "add read event fd [%d] failed: [%s].", m_fd, strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }
    m_fd_added = true;

    return RC_OK;
}

RetCode TcpClientImpl::WriteAsync(const void* buf, uint64_t sz, void* tag, NotificationQueue* nq) {
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    ev.data.ptr = static_cast<EventHandler*>(&m_written_handler);

    auto q = (NotificationQueueImpl*)nq;
    uint32_t op = m_written_fd_added ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    int err = epoll_ctl(q->m_epfd, op, m_written_fd, &ev);
    if (err) {
        logger_error(m_logger, "add write event failed: [%s].", strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }
    m_written_fd_added = true;

    int64_t wr_res = write(m_fd, buf, sz);
    if (wr_res < 0) {
        wr_res = -errno;
    }

    m_written_handler.SetParameters(tag, wr_res);

    const uint64_t value = 1;
    auto ret = write(m_written_fd, &value, sizeof(value));
    if (ret != sizeof(value)) {
        logger_error(m_logger, "register written event failed: [%s].", strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }

    return RC_OK;
}

RetCode TcpClientImpl::ShutDownAsync(void* tag, NotificationQueue* nq) {
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    ev.data.ptr = static_cast<EventHandler*>(&m_shutdown_handler);

    auto q = (NotificationQueueImpl*)nq;
    uint32_t op = m_shutdown_fd_added ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    int err = epoll_ctl(q->m_epfd, op, m_shutdown_fd, &ev);
    if (err) {
        logger_error(m_logger, "add shutdown event failed: [%s].", strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }
    m_shutdown_fd_added = true;

    int res = shutdown(m_fd, SHUT_RDWR);
    if (res != 0) {
        res = -errno;
    }

    close(m_fd);
    m_fd = -1;

    m_shutdown_handler.SetParameters(tag, res);

    const uint64_t value = 1;
    auto ret = write(m_shutdown_fd, &value, sizeof(value));
    if (ret != sizeof(value)) {
        logger_error(m_logger, "register shutdown event failed: [%s].", strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }

    return RC_OK;
}

}}
