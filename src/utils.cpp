#include "netkit/utils.h"
#include <cstring> // memset()
#include <cstdio> // snprintf()
#include <netdb.h>
#include <unistd.h> // close()
#include <arpa/inet.h>
#include <sys/timerfd.h>
using namespace std;

namespace netkit { namespace utils {

static int GetHostInfo(const char* host, uint16_t port, struct addrinfo** info,
                       Logger* logger) {
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
        return err;
    }

    return 0;
}

static int SetReuse(int fd, Logger* logger) {
    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) != 0) {
        logger_error(logger, "setsockopt failed: %s.", strerror(errno));
        return -errno;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(int)) != 0) {
        logger_error(logger, "setsockopt failed: %s.", strerror(errno));
        return -errno;
    }
    return 0;
}

int CreateTcpServerFd(const char* host, uint16_t port, Logger* logger) {
    int fd;
    int sc = 0;
    struct addrinfo* info = nullptr;

    sc = GetHostInfo(host, port, &info, logger);
    if (sc != 0) {
        return sc;
    }

    fd = socket(info->ai_family, info->ai_socktype | SOCK_CLOEXEC,
                info->ai_protocol);
    if (fd < 0) {
        logger_error(logger, "socket() failed: %s.", strerror(errno));
        goto err;
    }

    if (SetReuse(fd, logger) != 0) {
        goto err1;
    }

    if (bind(fd, info->ai_addr, info->ai_addrlen) != 0) {
        logger_error(logger, "bind failed: %s.", strerror(errno));
        goto err1;
    }

    if (listen(fd, 128) == -1) {
        logger_error(logger, "listen failed: %s.", strerror(errno));
        goto err1;
    }

    freeaddrinfo(info);
    return fd;

err1:
    close(fd);
err:
    freeaddrinfo(info);
    return -errno;
}

int CreateTcpClientFd(const char* host, uint16_t port, Logger* logger) {
    struct addrinfo* info = nullptr;
    auto ret = GetHostInfo(host, port, &info, logger);
    if (ret != 0) {
        return ret;
    }

    int fd = socket(info->ai_family, info->ai_socktype | SOCK_CLOEXEC,
                    info->ai_protocol);
    if (fd == -1) {
        logger_error(logger, "socket() failed: %s", strerror(errno));
        ret = -errno;
        goto err;
    }

    if (connect(fd, info->ai_addr, info->ai_addrlen) != 0) {
        logger_error(logger, "connect() failed: %s", strerror(errno));
        ret = -errno;
        goto err1;
    }

    freeaddrinfo(info);
    return fd;

err1:
    close(fd);
err:
    freeaddrinfo(info);
    return ret;
}

int CreateTimerFd(const TimeVal& interval, Logger* logger) {
    if (interval.tv_sec == 0 && interval.tv_usec == 0) {
        logger_error(logger, "interval == 0 is not allowed.");
        return -EINVAL;
    }

    int fd = timerfd_create(CLOCK_BOOTTIME, TFD_NONBLOCK | TFD_CLOEXEC);
    if (fd < 0) {
        logger_error(logger, "create timerfd failed: [%s].", strerror(errno));
        return -errno;
    }

    const struct itimerspec ts = {
        .it_interval =
            {
                .tv_sec = interval.tv_sec,
                .tv_nsec = interval.tv_usec * 1000,
            },
        .it_value =
            {
                .tv_sec = interval.tv_sec,
                .tv_nsec = interval.tv_usec * 1000,
            },
    };

    int err = timerfd_settime(fd, 0, &ts, nullptr);
    if (err) {
        logger_error(logger, "timerfd_settime failed: [%s].", strerror(errno));
        close(fd);
        return -errno;
    }

    return fd;
}

void GenEndpointInfo(int fd, EndpointInfo* info) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int ret = getpeername(fd, (struct sockaddr*)&addr, &len);
    if (ret == 0) {
        info->remote_addr = inet_ntoa(addr.sin_addr);
        info->remote_port = ntohs(addr.sin_port);
    }

    len = sizeof(addr);
    ret = getsockname(fd, (struct sockaddr*)&addr, &len);
    if (ret == 0) {
        info->local_addr = inet_ntoa(addr.sin_addr);
        info->local_port = ntohs(addr.sin_port);
    }
}

}}
