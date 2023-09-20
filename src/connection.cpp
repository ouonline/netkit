#include "netkit/connection.h"
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#include <memory>
using namespace std;

namespace netkit {

Connection::Connection(int fd, Logger* logger) {
    m_fd = fd;
    m_logger = logger;
    pthread_mutex_init(&m_lock, nullptr);

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

Connection::~Connection() {
    pthread_mutex_destroy(&m_lock);
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

RetCode Connection::Send(const void* data, uint64_t size, uint64_t* bytes_left) {
    if (size > 0) {
        pthread_mutex_lock(&m_lock);
        shared_ptr<void> __unlocker(nullptr, [this](void*) -> void {
            pthread_mutex_unlock(&m_lock);
        });

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

        if ((uint64_t)nbytes < size) {
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
