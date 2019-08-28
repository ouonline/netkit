#include "internal_server.h"
#include "internal_client.h"
#include "logger.h"
#include <cstring>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>

namespace utils { namespace net { namespace tcp {

StatusCode InternalServer::SetNonBlocking(int fd) {
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

StatusCode InternalServer::In() {
    InternalClient* client = nullptr;

    int fd = accept(m_fd, nullptr, nullptr);
    if (fd == -1) {
        log_error("accept failed: %s.", strerror(errno));
        return SC_INTERNAL_NET_ERR;
    }

    if (SetNonBlocking(fd) != SC_OK) {
        goto err;
    }

    client = new InternalClient(fd, m_factory, m_tp);
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

}}}
