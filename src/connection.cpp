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
    m_send_timeout = 0;

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

static RetCode RealSetSendTimeout(int fd, uint32_t ms, Logger* logger) {
    if (ms == 0) {
        return RC_SUCCESS;
    }

    struct timeval t;
    Time2Timeval(ms, &t);

    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof(t)) != 0) {
        logger_error(logger, "set send timeout failed: %s", strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }

    return RC_SUCCESS;
}

void Connection::SetSendTimeout(uint32_t ms) {
    m_send_timeout = ms;
}

static uint64_t DiffTimeMs(struct timeval end, const struct timeval& begin) {
    if (end.tv_usec < begin.tv_usec) {
        --end.tv_sec;
        end.tv_usec += 1000000;
    }

    return (end.tv_sec - begin.tv_sec) * 1000 + (end.tv_usec - begin.tv_usec) / 1000;
}

int Connection::Send(const void* data, uint32_t size) {
    std::unique_lock<std::mutex> __guard(m_lock);

    struct timeval begin;
    if (m_send_timeout > 0) {
        if (RealSetSendTimeout(m_fd, m_send_timeout, m_logger) != RC_SUCCESS) {
            return RC_INTERNAL_NET_ERR;
        }

        gettimeofday(&begin, NULL);
    }

    const char* cursor = (const char*)data;
    uint32_t bytes_to_send = size;

    while (bytes_to_send > 0) {
        int nbytes = send(m_fd, cursor, bytes_to_send, 0);
        if (nbytes < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                return RC_INTERNAL_NET_ERR;
            }

            usleep(1000);
        } else {
            bytes_to_send -= nbytes;
            cursor += nbytes;
        }

        if (m_send_timeout > 0) {
            struct timeval end;
            gettimeofday(&end, NULL);

            uint32_t time_cost = DiffTimeMs(end, begin);
            if (time_cost >= m_send_timeout) {
                break;
            }

            if (RealSetSendTimeout(m_fd, m_send_timeout - time_cost, m_logger) != RC_SUCCESS) {
                break;
            }
        }
    }

    return (size - bytes_to_send);
}

} // namespace netkit
