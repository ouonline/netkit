#include "processor_manager.h"
#include "internal_server.h"
#include "internal_client.h"
#include "logger.h"
#include <cstring>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
using namespace std;

namespace utils { namespace net { namespace tcp {

#include <netdb.h>

#define MAX_EVENTS 64

StatusCode ProcessorManager::Init() {
    if (m_epfd > 0) {
        return SC_OK;
    }

    m_thread_pool.AddThread(5);

    m_epfd = epoll_create(MAX_EVENTS);
    if (m_epfd < 0) {
        log_error("create epoll failed: %s.", strerror(errno));
        return SC_INTERNAL_NET_ERR;
    }

    return SC_OK;
}

StatusCode ProcessorManager::GetHostInfo(const char* host, uint16_t port,
                                         struct addrinfo** svr) {
    int err;
    char buf[8];
    struct addrinfo hints;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    snprintf(buf, 6, "%u", port);

    err = getaddrinfo(host, buf, &hints, svr);
    if (err) {
        log_error("getaddrinfo() failed: %s.", gai_strerror(err));
        return SC_INTERNAL_NET_ERR;
    }

    return SC_OK;
}

void ProcessorManager::Time2Timeval(uint32_t ms, struct timeval* t) {
    if (ms >= 1000) {
        t->tv_sec = ms / 1000;
        t->tv_usec = (ms % 1000) * 1000;
    } else {
        t->tv_sec = 0;
        t->tv_usec = ms * 1000;
    }
}

StatusCode ProcessorManager::SetSendTimeout(int fd, uint32_t ms) {
    struct timeval t;
    Time2Timeval(ms, &t);

    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof(t)) != 0) {
        log_error("setsockopt failed: %s.", strerror(errno));
        return SC_INTERNAL_NET_ERR;
    }

    return SC_OK;
}

StatusCode ProcessorManager::SetRecvTimeout(int fd, uint32_t ms) {
    struct timeval t;
    Time2Timeval(ms, &t);

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t)) != 0) {
        log_error("setsockopt failed: %s.", strerror(errno));
        return SC_INTERNAL_NET_ERR;
    }

    return SC_OK;
}

StatusCode ProcessorManager::SetReuseAddr(int fd) {
    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) != 0) {
        log_error("setsockopt failed: %s.", strerror(errno));
        return SC_INTERNAL_NET_ERR;
    }

    return SC_OK;
}

int ProcessorManager::CreateServerFd(const char* host, uint16_t port) {
    int fd;
    StatusCode sc = SC_INTERNAL_NET_ERR;
    struct addrinfo* info = nullptr;

    sc = GetHostInfo(host, port, &info);
    if (sc != SC_OK) {
        return -1;
    }

    fd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (fd < 0) {
        log_error("socket() failed: %s.", strerror(errno));
        goto err;
    }

    if (SetReuseAddr(fd) != SC_OK) {
        goto err1;
    }

    if (bind(fd, info->ai_addr, info->ai_addrlen) != 0) {
        log_error("bind failed: %s.", strerror(errno));
        goto err1;
    }

    if (listen(fd, 0) == -1) {
        log_error("listen failed: %s.", strerror(errno));
        goto err1;
    }

    freeaddrinfo(info);
    return fd;

err1:
    close(fd);
err:
    freeaddrinfo(info);
    return -1;
}

// TODO set connect/send/recv timeout
int ProcessorManager::CreateClientFd(const char* host, uint16_t port) {
    struct addrinfo* info = nullptr;
    if (GetHostInfo(host, port, &info) != SC_OK) {
        return -1;
    }

    int fd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (fd == -1) {
        log_error("socket() failed: %s", strerror(errno));
        goto err;
    }

    if (connect(fd, info->ai_addr, info->ai_addrlen) != 0) {
        log_error("connect() failed: %s", strerror(errno));
        goto err1;
    }

    freeaddrinfo(info);
    return fd;

err1:
    close(fd);
err:
    freeaddrinfo(info);
    return -1;
}

StatusCode ProcessorManager::SetNonBlocking(int fd) {
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

StatusCode ProcessorManager::AddServer(const char* addr, uint16_t port,
                                       const shared_ptr<ProcessorFactory>& factory) {
    int fd = CreateServerFd(addr, port);
    if (fd < 0) {
        log_error("create server failed.");
        return SC_INTERNAL_NET_ERR;
    }

    auto svr = new InternalServer(m_epfd, fd, factory, &m_thread_pool);
    if (!svr) {
        log_error("allocate tcp server failed.");
        close(fd);
        return SC_NOMEM;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = svr;
    if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &ev) != 0) {
        log_error("add server[%s:%u] to epoll failed: %s.", addr, port, strerror(errno));
        close(fd);
        delete svr;
        return SC_INTERNAL_NET_ERR;
    }

    return SC_OK;
}

StatusCode ProcessorManager::AddClient(const char* addr, uint16_t port,
                                       const shared_ptr<ProcessorFactory>& factory) {
    InternalClient* client = nullptr;

    int fd = CreateClientFd(addr, port);
    if (fd < 0) {
        log_error("create client failed.");
        return SC_INTERNAL_NET_ERR;
    }

    if (SetNonBlocking(fd) != SC_OK) {
        goto err;
    }

    client = new InternalClient(fd, factory, &m_thread_pool);
    if (!client) {
        log_error("allocate client failed.");
        goto err;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLET;
    ev.data.ptr = client;
    if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &ev) != 0) {
        log_error("add client failed: %s.", strerror(errno));
        goto err1;
    }

    return SC_OK;

err1:
    delete client;
err:
    close(fd);
    return SC_INTERNAL_NET_ERR;
}

StatusCode ProcessorManager::Run() {
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
                close(fd);
                epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, nullptr);
                delete e;
            }
        }
    }

    return SC_OK;
}

}}}
