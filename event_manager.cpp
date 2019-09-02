#include "event_manager.h"
#include "deps/logger/global_logger.h"
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
using namespace std;

#define MAX_EVENTS 64

namespace utils { namespace net {

StatusCode EventManager::Init() {
    m_epfd = epoll_create(MAX_EVENTS);
    if (m_epfd < 0) {
        log_error("create epoll failed: %s.", strerror(errno));
        return SC_INTERNAL_NET_ERR;
    }

    return SC_OK;
}

StatusCode EventManager::SetNonBlocking(int fd) {
    int opt;

    opt = fcntl(fd, F_GETFL);
    if (opt < 0) {
        log_error("fcntl failed: %s.", strerror(errno));
        return SC_INTERNAL_NET_ERR;
    }

    opt |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, opt) == -1) {
        log_error("fcntl failed: %s.", strerror(errno));
        return SC_INTERNAL_NET_ERR;
    }

    return SC_OK;
}

StatusCode EventManager::AddClient(EventHandler* e) {
    int fd = e->GetFd();
    if (SetNonBlocking(fd) != SC_OK) {
        return SC_INTERNAL_NET_ERR;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLET;
    ev.data.ptr = e;
    if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &ev) != 0) {
        return SC_INTERNAL_NET_ERR;
    }

    return SC_OK;
}

StatusCode EventManager::AddServer(EventHandler* e) {
    int fd = e->GetFd();
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = e;
    if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &ev) != 0) {
        return SC_INTERNAL_NET_ERR;
    }

    return SC_OK;
}

StatusCode EventManager::Loop() {
    struct epoll_event eventlist[MAX_EVENTS];
    while (true) {
        int i;
        int nfds = epoll_wait(m_epfd, eventlist, MAX_EVENTS, -1);
        if (nfds < 0) {
            if (errno == EINTR) {
                continue;
            }

            log_error("epoll_wait failed: %s", strerror(errno));
            return SC_INTERNAL_NET_ERR;
        }

        for (i = 0; i < nfds; ++i) {
            uint32_t events = eventlist[i].events;
            EventHandler* e = (EventHandler*)(eventlist[i].data.ptr);
            StatusCode sc = SC_OK;

            if ((events & EPOLLHUP) ||
                (events & EPOLLRDHUP) ||
                (events & EPOLLERR)) {
                e->Error();
                sc = SC_CLIENT_DISCONNECTED;
            } else {
                if (events & EPOLLIN) {
                    sc = e->In();
                }

                if (events & EPOLLOUT) {
                    if (sc == SC_OK) {
                        sc = e->Out();
                    }
                }
            }

            if (sc != SC_OK) {
                int fd = e->GetFd();
                epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, nullptr);
                close(fd);
                delete e;
            }
        }
    }

    return SC_OK;
}

}}
