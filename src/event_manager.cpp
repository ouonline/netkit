#include "netkit/event_manager.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
using namespace std;

#define MAX_EVENTS 64

namespace outils { namespace net {

StatusCode EventManager::Init() {
    m_epfd = epoll_create(MAX_EVENTS);
    if (m_epfd < 0) {
        logger_error(m_logger, "create epoll failed: %s.", strerror(errno));
        return SC_INTERNAL_NET_ERR;
    }

    return SC_OK;
}

StatusCode EventManager::AddHandler(EventHandler* e, unsigned int event) {
    int fd = e->GetFd();
    struct epoll_event ev;
    ev.events = event;
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

            logger_error(m_logger, "epoll_wait failed: %s", strerror(errno));
            return SC_INTERNAL_NET_ERR;
        }

        for (i = 0; i < nfds; ++i) {
            uint32_t events = eventlist[i].events;
            EventHandler* e = (EventHandler*)(eventlist[i].data.ptr);
            StatusCode sc = SC_OK;

            if ((events & EPOLLHUP) || (events & EPOLLRDHUP) || (events & EPOLLERR)) {
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

}} // namespace outils::net
