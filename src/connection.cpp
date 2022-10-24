#include "netkit/connection.h"
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <arpa/inet.h>
using namespace std;

namespace netkit {

Connection::Connection(int fd, Logger* logger) {
    m_fd = fd;
    m_logger = logger;

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int ret = getpeername(fd, (struct sockaddr*)&addr, &len);
    if (ret == 0) {
        m_info.remote_addr = inet_ntoa(addr.sin_addr);
        m_info.remote_port = addr.sin_port;
    }

    len = sizeof(addr);
    ret = getsockname(fd, (struct sockaddr*)&addr, &len);
    if (ret == 0) {
        m_info.local_addr = inet_ntoa(addr.sin_addr);
        m_info.local_port = addr.sin_port;
    }
}

static void Time2Timeval(uint32_t ms, struct timeval* t) {
    if (ms >= 1000) {
        t->tv_sec = ms / 1000;
        t->tv_usec = (ms % 1000) * 1000;
    } else {
        t->tv_sec = 0;
        t->tv_usec = ms * 1000;
    }
}

RetCode Connection::SetSendTimeout(uint32_t ms) {
    if (ms > 0) {
        struct timeval t;
        Time2Timeval(ms, &t);

        if (setsockopt(m_fd, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof(t)) != 0) {
            logger_error(m_logger, "set send timeout failed: %s.", strerror(errno));
            return RC_INTERNAL_NET_ERR;
        }
    }

    return RC_SUCCESS;
}

static uint64_t DiffTimeMs(struct timeval end, const struct timeval& begin) {
    if (end.tv_usec < begin.tv_usec) {
        --end.tv_sec;
        end.tv_usec += 1000000;
    }

    return (end.tv_sec - begin.tv_sec) * 1000 + (end.tv_usec - begin.tv_usec) / 1000;
}

RetCode Connection::Send(const void* data, uint32_t size, uint32_t* bytes_left) {
    if (size > 0) {
        std::unique_lock<std::mutex> __guard(m_lock);

        int nbytes = send(m_fd, data, size, 0);
        if (nbytes == -1) {
            if (bytes_left) {
                *bytes_left = size;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                logger_error(m_logger, "send timeout. nothing is sent.");
                return RC_TIMEOUT;
            }

            logger_error(m_logger, "send [%u] bytes failed: %s.", size, strerror(errno));
            return RC_INTERNAL_NET_ERR;
        }

        if (nbytes < size) {
            if (bytes_left) {
                *bytes_left = size - nbytes;
            }
            logger_error(m_logger, "send timeout. [%u] of [%u] bytes are sent.", nbytes, size);
            return RC_TIMEOUT;
        }
    }

    if (bytes_left) {
        *bytes_left = 0;
    }

    return RC_SUCCESS;
}

} // namespace netkit
