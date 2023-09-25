#include "utils.h"
#include <cstring> // memset()
#include <cstdio> // snprintf()
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h> // close()
#include <arpa/inet.h>
using namespace std;

namespace netkit { namespace utils {

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

static RetCode SetCloseOnExec(int fd, Logger* logger) {
    int flags = fcntl(fd, F_GETFD);
    if (flags == -1) {
        logger_error(logger, "fcntl(F_GETFD) failed: [%s].", strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }

    flags |= FD_CLOEXEC;
    int ret = fcntl(fd, F_SETFD);
    if (ret == -1) {
        logger_error(logger, "fcntl(F_SETFD) failed: [%s].", strerror(errno));
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
    if (SetCloseOnExec(fd, logger) != RC_OK) {
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
    close(fd);
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

    if (SetCloseOnExec(fd, logger) != RC_OK) {
        goto err1;
    }

    freeaddrinfo(info);
    return fd;

err1:
    shutdown(fd, SHUT_RDWR);
    close(fd);
err:
    freeaddrinfo(info);
    return -1;
}

RetCode SetNonBlocking(int fd, Logger* logger) {
    int opt;

    opt = fcntl(fd, F_GETFL);
    if (opt < 0) {
        logger_error(logger, "fcntl failed: %s.", strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }

    opt |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, opt) == -1) {
        logger_error(logger, "fcntl failed: %s.", strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }

    return RC_OK;
}

void GenConnectionInfo(int fd, ConnectionInfo* info) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int ret = getpeername(fd, (struct sockaddr*)&addr, &len);
    if (ret == 0) {
        info->remote_addr = inet_ntoa(addr.sin_addr);
        info->remote_port = addr.sin_port;
    }

    len = sizeof(addr);
    ret = getsockname(fd, (struct sockaddr*)&addr, &len);
    if (ret == 0) {
        info->local_addr = inet_ntoa(addr.sin_addr);
        info->local_port = addr.sin_port;
    }
}

}}
