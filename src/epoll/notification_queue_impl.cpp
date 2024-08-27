#include "netkit/epoll/notification_queue_impl.h"
#include <sys/socket.h> // accept()
#include <unistd.h> // close()
#include <cstring> // strerror()
#include <sys/eventfd.h>
#include <errno.h>

namespace netkit { namespace epoll {

int NotificationQueueImpl::Init(Logger* l) {
    if (m_logger) {
        return 0;
    }

    m_epfd = epoll_create1(EPOLL_CLOEXEC);
    if (m_epfd < 0) {
        logger_error(l, "create epoll failed: %s.", strerror(errno));
        return -errno;
    }

    m_logger = l;

    return 0;
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

static int DoEpollUpdate(int epfd, uint32_t flags, EventHandler* handler, int fd, Logger* logger) {
    struct epoll_event ev;
    ev.events = flags;
    ev.data.ptr = handler;

    auto ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    if (ret != 0) {
        if (errno == EEXIST) {
            ret = epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
            if (ret != 0) {
                logger_error(logger, "epoll mod server fd [%d] failed: [%s].", fd, strerror(errno));
                return -errno;
            }
        } else {
            logger_error(logger, "epoll add server fd [%d] failed: [%s].", fd, strerror(errno));
            return -errno;
        }
    }

    return 0;
}

struct AcceptHandler final : public EventHandler {
public:
    AcceptHandler(int svr_fd, void* t, bool keep_alive) : EventHandler(svr_fd, t, keep_alive) {}
    int64_t In() override {
        int cfd = accept(fd, nullptr, nullptr);
        if (cfd <= 0) {
            return -errno;
        }
        return cfd;
    }
};

int NotificationQueueImpl::MultiAcceptAsync(int64_t fd, void* tag) {
    auto handler = new AcceptHandler(fd, tag, true);
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = static_cast<EventHandler*>(handler);

    auto ret = epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &ev);
    if (ret != 0) {
        logger_error(m_logger, "epoll add server fd [%d] failed: [%s].", fd, strerror(errno));
        delete handler;
        return -errno;
    }

    return 0;
}

int NotificationQueueImpl::AcceptAsync(int64_t fd, void* tag) {
    auto handler = new AcceptHandler(fd, tag, false);
    auto ret = DoEpollUpdate(m_epfd, EPOLLIN | EPOLLONESHOT, handler, fd, m_logger);
    if (ret != 0) {
        logger_error(m_logger, "DoEpollUpdate in AcceptAsync() failed.");
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

int NotificationQueueImpl::ReadAsync(int64_t fd, void* buf, uint64_t sz, void* tag) {
    auto handler = new ReadHandler(fd, buf, sz, tag);
    auto ret = DoEpollUpdate(m_epfd, EPOLLIN | EPOLLONESHOT, handler, fd, m_logger);
    if (ret != 0) {
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

int NotificationQueueImpl::WriteAsync(int64_t fd, const void* buf, uint64_t sz, void* tag) {
    auto handler = new WriteHandler(fd, buf, sz, tag);
    auto ret = DoEpollUpdate(m_epfd, EPOLLOUT | EPOLLONESHOT, handler, fd, m_logger);
    if (ret != 0) {
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

int NotificationQueueImpl::CloseAsync(int64_t fd, void* tag) {
    int efd = eventfd(0, EFD_CLOEXEC);
    if (efd < 0) {
        logger_error(m_logger, "create eventfd failed: [%s].", strerror(errno));
        return -errno;
    }

    auto handler = new EventfdHandler(efd, tag);
    auto ret = DoEpollUpdate(m_epfd, EPOLLIN | EPOLLONESHOT, handler, efd, m_logger);
    if (ret != 0) {
        logger_error(m_logger, "DoEpollUpdate in CloseAsync() failed.");
        delete handler;
        return ret;
    }

    handler->res = close(fd);
    const uint64_t v = 1;
    write(efd, &v, sizeof(v));

    return 0;
}

int NotificationQueueImpl::NotifyAsync(NotificationQueueImpl* nq, int res, void* tag) {
    int efd = eventfd(0, EFD_CLOEXEC);
    if (efd < 0) {
        logger_error(m_logger, "create eventfd failed: [%s].", strerror(errno));
        return -errno;
    }

    auto handler = new EventfdHandler(efd, tag);
    auto ret = DoEpollUpdate(nq->m_epfd, EPOLLIN | EPOLLONESHOT, handler, efd, m_logger);
    if (ret != 0) {
        logger_error(m_logger, "DoEpollUpdate in NotifyAsync() failed.");
        delete handler;
        return ret;
    }

    handler->res = res;
    const uint64_t v = 1;
    write(efd, &v, sizeof(v));

    return 0;
}

int NotificationQueueImpl::Next(int64_t* res, void** tag, TimeVal* timeout) {
    if (m_event_idx >= m_nr_valid_event) {
        int ts;
        if (timeout) {
            if (timeout->tv_sec == 0 && timeout->tv_usec == 0) {
                ts = 0;
            } else {
                ts = timeout->tv_sec * 1000000 + timeout->tv_usec;
            }
        } else {
            ts = -1; // infinitely
        }
        m_nr_valid_event = epoll_wait(m_epfd, m_event_list, MAX_EVENTS, ts);
        if (m_nr_valid_event < 0) {
            if (errno != EAGAIN) {
                logger_error(m_logger, "epoll_wait failed: %s", strerror(errno));
            }
            return -errno;
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

    return 0;
}

}}
