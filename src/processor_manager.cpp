#include "netkit/processor_manager.h"
#include "internal_server.h"
#include "internal_client.h"
#include "utils.h"
#include <cstring>
#include <unistd.h>
#include <errno.h>
using namespace std;

namespace netkit { namespace tcp {

#include <netdb.h>

RetCode ProcessorManager::Init() {
    m_thread_pool.AddThread(5);
    return m_event_mgr.Init();
}

RetCode ProcessorManager::GetHostInfo(const char* host, uint16_t port, struct addrinfo** svr) {
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
        logger_error(m_logger, "getaddrinfo() failed: %s.", gai_strerror(err));
        return RC_INTERNAL_NET_ERR;
    }

    return RC_SUCCESS;
}

RetCode ProcessorManager::SetReuseAddr(int fd) {
    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) != 0) {
        logger_error(m_logger, "setsockopt failed: %s.", strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }

    return RC_SUCCESS;
}

int ProcessorManager::CreateServerFd(const char* host, uint16_t port) {
    int fd;
    RetCode sc = RC_INTERNAL_NET_ERR;
    struct addrinfo* info = nullptr;

    sc = GetHostInfo(host, port, &info);
    if (sc != RC_SUCCESS) {
        return -1;
    }

    fd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (fd < 0) {
        logger_error(m_logger, "socket() failed: %s.", strerror(errno));
        goto err;
    }

    if (SetReuseAddr(fd) != RC_SUCCESS) {
        goto err1;
    }

    if (bind(fd, info->ai_addr, info->ai_addrlen) != 0) {
        logger_error(m_logger, "bind failed: %s.", strerror(errno));
        goto err1;
    }

    if (listen(fd, 0) == -1) {
        logger_error(m_logger, "listen failed: %s.", strerror(errno));
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
    if (GetHostInfo(host, port, &info) != RC_SUCCESS) {
        return -1;
    }

    int fd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (fd == -1) {
        logger_error(m_logger, "socket() failed: %s", strerror(errno));
        goto err;
    }

    if (connect(fd, info->ai_addr, info->ai_addrlen) != 0) {
        logger_error(m_logger, "connect() failed: %s", strerror(errno));
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

RetCode ProcessorManager::AddServer(const char* addr, uint16_t port, const shared_ptr<ProcessorFactory>& factory) {
    int fd = CreateServerFd(addr, port);
    if (fd < 0) {
        logger_error(m_logger, "create server failed.");
        return RC_INTERNAL_NET_ERR;
    }

    auto svr = new InternalServer(fd, factory, &m_event_mgr, &m_thread_pool, m_logger);
    if (!svr) {
        logger_error(m_logger, "allocate tcp server failed.");
        close(fd);
        return RC_NOMEM;
    }

    if (m_event_mgr.AddHandler(svr, EPOLLIN) != RC_SUCCESS) {
        logger_error(m_logger, "add server[%s:%u] to epoll failed: %s.", addr, port, strerror(errno));
        close(fd);
        delete svr;
        return RC_INTERNAL_NET_ERR;
    }

    return RC_SUCCESS;
}

RetCode ProcessorManager::AddClient(const char* addr, uint16_t port, const shared_ptr<ProcessorFactory>& factory) {
    int fd = CreateClientFd(addr, port);
    if (fd < 0) {
        logger_error(m_logger, "create client failed.");
        return RC_INTERNAL_NET_ERR;
    }
    if (SetNonBlocking(fd, m_logger) != RC_SUCCESS) {
        close(fd);
        return RC_INTERNAL_NET_ERR;
    }

    auto client = new InternalClient(fd, factory, &m_thread_pool, m_logger);
    if (!client) {
        logger_error(m_logger, "allocate client failed.");
        goto err;
    }

    if (m_event_mgr.AddHandler(client, EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLET) == RC_SUCCESS) {
        return RC_SUCCESS;
    }

    logger_error(m_logger, "add client failed: %s.", strerror(errno));
    delete client;
err:
    close(fd);
    return RC_INTERNAL_NET_ERR;
}

RetCode ProcessorManager::Run() {
    return m_event_mgr.Loop();
}

}} // namespace netkit::tcp
