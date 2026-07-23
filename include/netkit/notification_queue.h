#ifndef __NETKIT_NOTIFICATION_QUEUE_H__
#define __NETKIT_NOTIFICATION_QUEUE_H__

#include "timeval.h"
#include <stdint.h>

namespace netkit {

class NotificationQueue {
public:
    virtual ~NotificationQueue() = default;

    /**
       @brief accepts one connection. returns 0 or -errno.
    */
    virtual int AcceptAsync(uint32_t svr_fd, void* tag, bool multishot) = 0;

    /**
       @brief reads at most `sz` bytes into `buf` from `fd`. returns 0 or
       -errno.
    */
    virtual int ReadAsync(uint32_t fd, void* buf, uint32_t sz, void* tag) = 0;

    /**
       @brief writes at most `sz` bytes from `buf` to `fd`. returns 0 or -errno.
    */
    virtual int WriteAsync(uint32_t fd, const void* buf, uint32_t sz,
                           void* tag) = 0;

    /**
       @brief closes `fd`. returns 0 or -errno.
    */
    virtual int CloseAsync(uint32_t fd, void* tag) = 0;

    /**
       @brief notifies another notification queue about an event. returns 0 or
       -errno.
    */
    virtual int NotifyAsync(NotificationQueue* nq, int res, void* tag) = 0;

    /**
       @brief gets next event. returns 0 or -errno.

       @param `res` has different meanings according to events:
       - ACCEPT: `res` is the client fd or -errno, `tag` is the value passed to
                 `AcceptAsync()`.
       - READ: `res` is the number of bytes received or -errno, `tag` is the
               value passed to `ReadAsync()`.
       - WRITE: `res` is the number of bytes sent or -errno, `tag` is the
                value passed to `WriteAsync()`.
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
