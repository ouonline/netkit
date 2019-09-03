#include "processor_manager.h"
#include "internal_server.h"
#include "internal_client.h"
#include "logger/global_logger.h"
#include <cstring>
#include <unistd.h>
#include <errno.h>
using namespace std;

namespace utils { namespace net { namespace tcp {

#include <netdb.h>

StatusCode ProcessorManager::Init() {
    m_thread_pool.AddThread(5);
    return m_event_mgr.Init();
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

StatusCode ProcessorManager::AddServer(const char* addr, uint16_t port,
                                       const shared_ptr<ProcessorFactory>& factory) {
    int fd = CreateServerFd(addr, port);
    if (fd < 0) {
        log_error("create server failed.");
        return SC_INTERNAL_NET_ERR;
    }

    auto svr = new InternalServer(fd, factory, &m_event_mgr, &m_thread_pool);
    if (!svr) {
        log_error("allocate tcp server failed.");
        close(fd);
        return SC_NOMEM;
    }

    if (m_event_mgr.AddServer(svr) != SC_OK) {
        log_error("add server[%s:%u] to epoll failed: %s.", addr, port, strerror(errno));
        close(fd);
        delete svr;
        return SC_INTERNAL_NET_ERR;
    }

    return SC_OK;
}

StatusCode ProcessorManager::AddClient(const char* addr, uint16_t port,
                                       const shared_ptr<ProcessorFactory>& factory) {
    int fd = CreateClientFd(addr, port);
    if (fd < 0) {
        log_error("create client failed.");
        return SC_INTERNAL_NET_ERR;
    }

    auto client = new InternalClient(fd, factory, &m_thread_pool);
    if (!client) {
        log_error("allocate client failed.");
        goto err;
    }

    if (m_event_mgr.AddClient(client) == SC_OK) {
        return SC_OK;
    }

    log_error("add client failed: %s.", strerror(errno));
    delete client;
err:
    close(fd);
    return SC_INTERNAL_NET_ERR;
}

StatusCode ProcessorManager::Run() {
    return m_event_mgr.Loop();
}

}}}
