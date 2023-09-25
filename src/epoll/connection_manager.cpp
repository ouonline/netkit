#include "../utils.h"
#include "netkit/epoll/event_handler.h"
#include "netkit/epoll/connection_manager.h"
#include <cstring> // strerror()
#include <sys/socket.h> // shutdown()
#include <unistd.h> // close()
using namespace std;

namespace netkit { namespace epoll {

RetCode ConnectionManager::Init() {
    m_epfd = epoll_create(MAX_EVENTS);
    if (m_epfd < 0) {
        logger_error(m_logger, "create epoll failed: %s.", strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }

    return RC_OK;
}

ConnectionManager::~ConnectionManager() {
    if (m_epfd > 0) {
        close(m_epfd);
    }
}

class InternalServer final : public EventHandler {
public:
    InternalServer(void* t, int svr_fd) : EventHandler(t), m_fd(svr_fd) {}
    int64_t In() override {
        int fd = accept(m_fd, nullptr, nullptr);
        if (fd < 0) {
            return -errno;
        }
        return fd;
    }

private:
    int m_fd;
};

TcpServer ConnectionManager::CreateTcpServer(const char* addr, uint16_t port, void* tag) {
    int fd = utils::CreateTcpServerFd(addr, port, m_logger);
    if (fd < 0) {
        logger_error(m_logger, "create tcp server failed.");
        return TcpServer(-1);
    }

    auto rc = utils::SetNonBlocking(fd, m_logger);
    if (rc != RC_OK) {
        logger_error(m_logger, "SetNonBlocking failed.");
        goto err;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = static_cast<EventHandler*>(new InternalServer(tag, fd));
    if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &ev) != 0) {
        logger_error(m_logger, "epoll add server fd failed: [%s].", strerror(errno));
        goto err;
    }

    return TcpServer(fd);

err:
    shutdown(fd, SHUT_RDWR);
    close(fd);
    return TcpServer(-1);
}

RetCode ConnectionManager::InitializeConnection(int fd, Connection* c) {
    return c->Init(fd, m_epfd, m_logger);
}

RetCode ConnectionManager::CreateTcpClient(const char* addr, uint16_t port, Connection* c) {
    int fd = utils::CreateTcpClientFd(addr, port, m_logger);
    if (fd < 0) {
        logger_error(m_logger, "create tcp client failed.");
        return RC_INTERNAL_NET_ERR;
    }

    return InitializeConnection(fd, c);
}

RetCode ConnectionManager::Wait(int64_t* res, void** tag) {
    if (m_event_idx >= m_nr_valid_event) {
again:
        m_nr_valid_event = epoll_wait(m_epfd, m_event_list, MAX_EVENTS, -1);
        if (m_nr_valid_event < 0) {
            if (errno == EINTR) {
                goto again;
            }
            logger_error(m_logger, "epoll_wait failed: %s", strerror(errno));
            return RC_INTERNAL_NET_ERR;
        }

        m_event_idx = 0;
    }

    uint32_t events = m_event_list[m_event_idx].events;
    auto handler = static_cast<EventHandler*>(m_event_list[m_event_idx].data.ptr);

    *tag = handler->tag;

    if ((events & EPOLLHUP) || (events & EPOLLRDHUP) || (events & EPOLLERR)) {
        *res = 0;
    } else if (events & EPOLLIN) {
        *res = handler->In();
    }

    ++m_event_idx;

    return RC_OK;
}

}}
