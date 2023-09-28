#ifndef __NETKIT_NOTIFICATION_QUEUE_H__
#define __NETKIT_NOTIFICATION_QUEUE_H__

#include "netkit/retcode.h"
#include <stdint.h>

namespace netkit {

class NotificationQueue {
public:
    virtual ~NotificationQueue() {}

    /** @brief thread-safe */
    virtual RetCode MultiAcceptAsync(int64_t fd, void* tag) = 0;

    /** @brief thread-safe */
    virtual RetCode ReadAsync(int64_t fd, void* buf, uint64_t sz, void* tag) = 0;

    /** @brief thread-safe */
    virtual RetCode WriteAsync(int64_t fd, const void* buf, uint64_t sz, void* tag) = 0;

    /**
       @param `res` has different meanings according to events:
       - CONNECTED: `res` is the client fd or -errno, `tag` is the value passed to `MultiAcceptAsync()`
       - WRITE: `res` is the number of bytes sent or -errno, `tag` is the value passed to `WriteAsync()`.
       - READ: `res` is the number of bytes received or -errno, `tag` is the value passed to `ReadAsync()`.

       @note
       - `Wait()` is NOT thread-safe.
       - callers should handle failures when `res` is negative.
    */
    virtual RetCode Wait(int64_t* res, void** tag) = 0;
};

}

#endif
