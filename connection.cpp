#include "connection.h"
#include "logger.h"
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <arpa/inet.h>
using namespace std;

namespace utils { namespace net {

Connection::Connection(int fd)
    : m_fd(fd) {
    pthread_mutex_init(&m_lock, nullptr);

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int ret = getpeername(fd, (struct sockaddr*)&addr, &len);
    if (ret == 0) {
        m_info.addr = inet_ntoa(addr.sin_addr);
        m_info.port = addr.sin_port;
    }
}

Connection::~Connection() {
    pthread_mutex_destroy(&m_lock);
}

static inline void Time2Timeval(uint32_t ms, struct timeval* t) {
    if (ms >= 1000) {
        t->tv_sec = ms / 1000;
        t->tv_usec = (ms % 1000) * 1000;
    } else {
        t->tv_sec = 0;
        t->tv_usec = ms * 1000;
    }
}

StatusCode Connection::RealSetSendTimeout(uint32_t ms) {
    if (ms == 0) {
        return SC_OK;
    }

    struct timeval t;
    Time2Timeval(ms, &t);

    if (setsockopt(m_fd, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof(t)) != 0) {
        log_error("set send timeout failed: %s", strerror(errno));
        return SC_INTERNAL_NET_ERR;
    }

    return SC_OK;
}

static inline uint64_t DiffTimeMs(struct timeval end, const struct timeval& begin) {
    if (end.tv_usec < begin.tv_usec) {
        --end.tv_sec;
        end.tv_usec += 1000000;
    }

    return (end.tv_sec - begin.tv_sec) * 1000 +
        (end.tv_usec - begin.tv_usec) / 1000;
}

class LockGuard final {

public:
    LockGuard(pthread_mutex_t* lock) {
        m_lock = lock;
        pthread_mutex_lock(lock);
    }
    ~LockGuard() {
        pthread_mutex_unlock(m_lock);
    }

private:
    pthread_mutex_t* m_lock;
};

int Connection::Send(const char* data, int size, uint32_t timeout_ms) {
    LockGuard lg(&m_lock);

    struct timeval begin;
    if (timeout_ms > 0) {
        if (RealSetSendTimeout(timeout_ms) != SC_OK) {
            return SC_INTERNAL_NET_ERR;
        }

        gettimeofday(&begin, NULL);
    }

    const char* cursor = (const char*)data;
    uint32_t bytes_to_send = size;

    while (bytes_to_send > 0) {
        int nbytes = send(m_fd, cursor, bytes_to_send, 0);
        if (nbytes < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                return SC_INTERNAL_NET_ERR;
            }

            usleep(1000);
        } else {
            bytes_to_send -= nbytes;
            cursor += nbytes;
        }

        if (timeout_ms > 0) {
            struct timeval end;
            gettimeofday(&end, NULL);

            uint32_t time_cost = DiffTimeMs(end, begin);
            if (time_cost >= timeout_ms) {
                break;
            }

            if (RealSetSendTimeout(timeout_ms - time_cost) != SC_OK) {
                break;
            }
        }
    }

    return (size - bytes_to_send);
}

}}
