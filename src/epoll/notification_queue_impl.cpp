#include "netkit/epoll/notification_queue_impl.h"
#include <sys/socket.h> // accept()
#include <unistd.h> // close()
#include <cstring> // strerror()
#include <sys/eventfd.h>
#include <errno.h>

namespace netkit { namespace epoll {

RetCode NotificationQueueImpl::Init(Logger* l) {
    if (m_logger) {
        return RC_OK;
    }

    m_epfd = epoll_create1(EPOLL_CLOEXEC);
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
    EventHandler(int f, void* t, bool ka) : fd(f), tag(t), keep_alive(ka) {}
    virtual ~EventHandler() {}
    virtual int64_t In() {
        return -1;
    }
    virtual int64_t Out() {
        return -1;
    }

    int fd;
    void* tag;
    bool keep_alive;
};

static RetCode DoEpollUpdate(int epfd, uint32_t flags, EventHandler* handler, int fd, Logger* logger) {
    struct epoll_event ev;
    ev.events = flags;
    ev.data.ptr = handler;

    auto ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    if (ret != 0) {
        if (errno == EEXIST) {
            ret = epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
            if (ret != 0) {
                logger_error(logger, "epoll mod server fd [%d] failed: [%s].", fd, strerror(errno));
                return RC_INTERNAL_NET_ERR;
            }
        } else {
            logger_error(logger, "epoll add server fd [%d] failed: [%s].", fd, strerror(errno));
            return RC_INTERNAL_NET_ERR;
        }
    }

    return RC_OK;
}

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
    auto handler = new AcceptHandler(fd, tag);
    auto ret = DoEpollUpdate(m_epfd, EPOLLIN, handler, fd, m_logger);
    if (ret != RC_OK) {
        logger_error(m_logger, "DoEpollUpdate in MultiAcceptAsync() failed.");
        delete handler;
    }
    return ret;
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
    auto handler = new ReadHandler(fd, buf, sz, tag);
    auto ret = DoEpollUpdate(m_epfd, EPOLLIN | EPOLLONESHOT, handler, fd, m_logger);
    if (ret != RC_OK) {
        logger_error(m_logger, "DoEpollUpdate in ReadAsync() failed.");
        delete handler;
    }
    return ret;
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
    auto handler = new WriteHandler(fd, buf, sz, tag);
    auto ret = DoEpollUpdate(m_epfd, EPOLLOUT | EPOLLONESHOT, handler, fd, m_logger);
    if (ret != RC_OK) {
        logger_error(m_logger, "DoEpollUpdate in WriteAsync() failed.");
        delete handler;
    }
    return ret;
}

struct EventfdHandler final : public EventHandler {
public:
    EventfdHandler(int fd, void* t) : EventHandler(fd, t, false) {}
    ~EventfdHandler() {
        close(fd);
    }
    int64_t In() override {
        return res;
    }

    int64_t res;
};

RetCode NotificationQueueImpl::CloseAsync(int64_t fd, void* tag) {
    int efd = eventfd(0, EFD_CLOEXEC);
    if (efd < 0) {
        logger_error(m_logger, "create eventfd failed: [%s].", strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }

    auto handler = new EventfdHandler(efd, tag);
    auto ret = DoEpollUpdate(m_epfd, EPOLLIN | EPOLLONESHOT, handler, efd, m_logger);
    if (ret != RC_OK) {
        logger_error(m_logger, "DoEpollUpdate in CloseAsync() failed.");
        delete handler;
        return ret;
    }

    handler->res = close(fd);
    const uint64_t v = 1;
    write(efd, &v, sizeof(v));

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

    *tag = handler->tag;

    if ((events & EPOLLHUP) || (events & EPOLLRDHUP) || (events & EPOLLERR)) {
        *res = 0;
        epoll_ctl(m_epfd, EPOLL_CTL_DEL, handler->fd, nullptr);
        delete handler;
    } else if (events & EPOLLIN) {
        *res = handler->In();
        if (!handler->keep_alive) {
            delete handler;
        }
    } else if (events & EPOLLOUT) {
        *res = handler->Out();
        delete handler;
    }

    return RC_OK;
}

}}
