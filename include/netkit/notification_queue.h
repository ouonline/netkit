#ifndef __NETKIT_NOTIFICATION_QUEUE_H__
#define __NETKIT_NOTIFICATION_QUEUE_H__

#include "timeval.h"
#include <stdint.h>

namespace netkit {

class NotificationQueue {
public:
    virtual ~NotificationQueue() {}

    /**
       @brief accepts multiple connections. returns 0 or -errno.
    */
    virtual int MultiAcceptAsync(int64_t svr_fd, void* tag) = 0;

    /**
       @brief accepts one connection. returns 0 or -errno.
    */
    virtual int AcceptAsync(int64_t svr_fd, void* tag) = 0;

    /**
       @brief receives at most `sz` bytes into `buf` from `fd`. returns 0 or
       -errno.
    */
    virtual int RecvAsync(int64_t fd, void* buf, uint64_t sz, void* tag) = 0;

    /**
       @brief sends at most `sz` bytes from `buf` to `fd`, with an additional
       flag `MSG_NOSIGNAL`. returns 0 or -errno.
    */
    virtual int SendAsync(int64_t fd, const void* buf, uint64_t sz,
                          void* tag) = 0;

    /**
       @brief reads at most `sz` bytes into `buf` from `fd`. returns 0 or
       -errno.
    */
    virtual int ReadAsync(int64_t fd, void* buf, uint64_t sz, void* tag) = 0;

    /**
       @brief writes at most `sz` bytes from `buf` to `fd`. returns 0 or -errno.
    */
    virtual int WriteAsync(int64_t fd, const void* buf, uint64_t sz,
                           void* tag) = 0;

    /**
       @brief closes `fd`. returns 0 or -errno.
    */
    virtual int CloseAsync(int64_t fd, void* tag) = 0;

    /**
       @brief gets next event. returns 0 or -errno.

       @param `res` has different meanings according to events:
       - ACCEPT: `res` is the client fd or -errno, `tag` is the value passed to
                 `MultiAcceptAsync()` or `AcceptAsync()`.
       - RECV/READ: `res` is the number of bytes received or -errno, `tag` is
       the value passed to `RecvAsync()`.
       - SEND/WRITE: `res` is the number of bytes sent or -errno, `tag` is the
       value passed to `SendAsync()`.
       - CLOSE: `res` is the return value of `close()` or -errno, `tag` is the
       value passed to `CloseAsync()`.

       @param `timtout` waits until at least one event arrives, timeout reaches,
       or some error occurs. `nullptr` for blocking without timeout.

       @return -EAGAIN if there is no events, 0 for success, and -errno for
       other errors.
    */
    virtual int Next(int64_t* res, void** tag, const TimeVal* timeout) = 0;
};

}

#endif
