#include "netkit/epoll/notification_queue_impl.h"
#include <sys/socket.h> // accept()
#include <unistd.h> // close()
#include <cstring> // strerror()
#include <errno.h>

namespace netkit { namespace epoll {

RetCode NotificationQueueImpl::Init(Logger* l) {
    if (m_logger) {
        return RC_OK;
    }

    m_epfd = epoll_create(MAX_EVENTS);
    if (m_epfd < 0) {
        logger_error(l, "create epoll failed: %s.", strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }

    m_logger = l;

    return RC_OK;
}

void NotificationQueueImpl::Destroy() {
    if (m_epfd > 0) {
        close(m_epfd);
        m_epfd = -1;
        m_event_idx = 0;
        m_nr_valid_event = 0;
    }
}

struct EventHandler {
    EventHandler(int f, void* t, bool is_svr) : fd(f), tag(t), is_server(is_svr) {}
    virtual ~EventHandler() {}
    virtual int64_t In() {
        return -1;
    }
    virtual int64_t Out() {
        return -1;
    }

    int fd;
    void* tag;
    bool is_server;
};

struct AcceptHandler final : public EventHandler {
public:
    AcceptHandler(int svr_fd, void* t) : EventHandler(svr_fd, t, true) {}
    int64_t In() override {
        int cfd = accept(fd, nullptr, nullptr);
        if (cfd <= 0) {
            return -errno;
        }
        return cfd;
    }
};

RetCode NotificationQueueImpl::MultiAcceptAsync(int64_t fd, void* tag) {
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = static_cast<EventHandler*>(new AcceptHandler(fd, tag));
    if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &ev) != 0) {
        logger_error(m_logger, "epoll add server fd failed: [%s].", strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }
    return RC_OK;
}

struct ReadHandler final : public EventHandler {
public:
    ReadHandler(int cfd, void* buf, uint64_t sz, void* t) : EventHandler(cfd, t, false), m_buf(buf), m_sz(sz) {}
    int64_t In() override {
        auto nbytes = read(fd, m_buf, m_sz);
        if (nbytes == -1) {
            return -errno;
        }
        return nbytes;
    }

private:
    void* m_buf;
    uint64_t m_sz;
};

RetCode NotificationQueueImpl::ReadAsync(int64_t fd, void* buf, uint64_t sz, void* tag) {
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLONESHOT;
    ev.data.ptr = static_cast<EventHandler*>(new ReadHandler(fd, buf, sz, tag));

    int err = epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &ev);
    if (err) {
        logger_error(m_logger, "add read event of fd [%ld] failed: [%s].", fd, strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }

    return RC_OK;
}

struct WriteHandler final : public EventHandler {
public:
    WriteHandler(int cfd, const void* buf, uint64_t sz, void* t) : EventHandler(cfd, t, false), m_buf(buf), m_sz(sz) {}
    int64_t Out() override {
        auto nbytes = write(fd, m_buf, m_sz);
        if (nbytes == -1) {
            return -errno;
        }
        return nbytes;
    }

private:
    const void* m_buf;
    uint64_t m_sz;
};

RetCode NotificationQueueImpl::WriteAsync(int64_t fd, const void* buf, uint64_t sz, void* tag) {
    struct epoll_event ev;
    ev.events = EPOLLOUT | EPOLLONESHOT;
    ev.data.ptr = static_cast<EventHandler*>(new WriteHandler(fd, buf, sz, tag));

    int err = epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &ev);
    if (err) {
        logger_error(m_logger, "add write event of fd [%ld] failed: [%s].", fd, strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }

    return RC_OK;
}

RetCode NotificationQueueImpl::Wait(int64_t* res, void** tag) {
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
    ++m_event_idx;

    if (!handler->is_server) {
        epoll_ctl(m_epfd, EPOLL_CTL_DEL, handler->fd, nullptr);
    }

    *tag = handler->tag;

    if ((events & EPOLLHUP) || (events & EPOLLRDHUP) || (events & EPOLLERR)) {
        *res = 0;
        if (handler->is_server) {
            epoll_ctl(m_epfd, EPOLL_CTL_DEL, handler->fd, nullptr);
        }
        delete handler;
    } else if (events & EPOLLIN) {
        *res = handler->In();
        if (!handler->is_server) {
            delete handler;
        }
    } else if (events & EPOLLOUT) {
        *res = handler->Out();
        delete handler;
    }


    return RC_OK;
}

}}
