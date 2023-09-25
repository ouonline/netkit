#include "netkit/epoll/read_handler.h"
#include "netkit/epoll/eventfd_handler.h"
#include "../utils.h"
#include "netkit/epoll/connection.h"
#include <arpa/inet.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <unistd.h> // close()
#include <cstring> // strerror()
using namespace std;

namespace netkit { namespace epoll {

Connection::~Connection() {
    if (m_fd > 0) {
        shutdown(m_fd, SHUT_RDWR);
        close(m_fd);
    }
    if (m_written_fd > 0) {
        close(m_written_fd);
    }
    if (m_shutdown_fd > 0) {
        close(m_shutdown_fd);
    }
}

RetCode Connection::Init(int fd, int epfd, Logger* logger) {
    m_fd = fd;
    m_epfd = epfd;
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
    m_read_handler.SetParameters(buf, sz, tag);

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    ev.data.ptr = static_cast<EventHandler*>(&m_read_handler);

    uint32_t op = m_fd_added ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    int err = epoll_ctl(m_epfd, op, m_fd, &ev);
    if (err) {
        logger_error(m_logger, "add read event fd [%d] failed: [%s].", m_fd, strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }
    m_fd_added = true;

    return RC_OK;
}

RetCode Connection::WriteAsync(const void* buf, uint64_t sz, void* tag) {
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    ev.data.ptr = static_cast<EventHandler*>(&m_written_handler);

    uint32_t op = m_written_fd_added ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    int err = epoll_ctl(m_epfd, op, m_written_fd, &ev);
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

RetCode Connection::ShutDownAsync(void* tag) {
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    ev.data.ptr = static_cast<EventHandler*>(&m_shutdown_handler);

    uint32_t op = m_shutdown_fd_added ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
    int err = epoll_ctl(m_epfd, op, m_shutdown_fd, &ev);
    if (err) {
        logger_error(m_logger, "add shutdown event failed: [%s].", strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }
    m_shutdown_fd_added = true;

    epoll_ctl(m_epfd, EPOLL_CTL_DEL, m_fd, nullptr);
    int res = shutdown(m_fd, SHUT_RDWR);
    if (res != 0) {
        res = -errno;
    }

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
