#include "netkit/retcode.h"
#include "utils.h"
#include <cstring> // memset()
#include <cstdio> // sprintf()
#include <errno.h>

namespace netkit { namespace utils {

#include <netdb.h>

static RetCode GetHostInfo(const char* host, uint16_t port, struct addrinfo** info, Logger* logger) {
    char buf[8];
    struct addrinfo hints;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    snprintf(buf, 6, "%u", port);

    int err = getaddrinfo(host, buf, &hints, info);
    if (err) {
        logger_error(logger, "getaddrinfo() failed: %s.", gai_strerror(err));
        return RC_INTERNAL_NET_ERR;
    }

    return RC_OK;
}

static RetCode SetReuseAddr(int fd, Logger* logger) {
    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) != 0) {
        logger_error(logger, "setsockopt failed: %s.", strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }

    return RC_OK;
}

int CreateTcpServerFd(const char* host, uint16_t port, Logger* logger) {
    int fd;
    RetCode sc = RC_INTERNAL_NET_ERR;
    struct addrinfo* info = nullptr;

    sc = GetHostInfo(host, port, &info, logger);
    if (sc != RC_OK) {
        return -1;
    }

    fd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (fd < 0) {
        logger_error(logger, "socket() failed: %s.", strerror(errno));
        goto err;
    }

    if (SetReuseAddr(fd, logger) != RC_OK) {
        goto err1;
    }

    if (bind(fd, info->ai_addr, info->ai_addrlen) != 0) {
        logger_error(logger, "bind failed: %s.", strerror(errno));
        goto err1;
    }

    if (listen(fd, 0) == -1) {
        logger_error(logger, "listen failed: %s.", strerror(errno));
        goto err1;
    }

    freeaddrinfo(info);
    return fd;

err1:
    shutdown(fd, SHUT_RDWR);
err:
    freeaddrinfo(info);
    return -1;
}

int CreateTcpClientFd(const char* host, uint16_t port, Logger* logger) {
    struct addrinfo* info = nullptr;
    if (GetHostInfo(host, port, &info, logger) != RC_OK) {
        return -1;
    }

    int fd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (fd == -1) {
        logger_error(logger, "socket() failed: %s", strerror(errno));
        goto err;
    }

    if (connect(fd, info->ai_addr, info->ai_addrlen) != 0) {
        logger_error(logger, "connect() failed: %s", strerror(errno));
        goto err1;
    }

    freeaddrinfo(info);
    return fd;

err1:
    shutdown(fd, SHUT_RDWR);
err:
    freeaddrinfo(info);
    return -1;
}

}}
