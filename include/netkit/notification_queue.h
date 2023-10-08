#ifndef __NETKIT_NOTIFICATION_QUEUE_H__
#define __NETKIT_NOTIFICATION_QUEUE_H__

#include "netkit/retcode.h"
#include <stdint.h>

namespace netkit {

class NotificationQueue {
public:
    virtual ~NotificationQueue() {}

    /** @brief use `CloseAsync()` to close `svr_fd`. */
    virtual RetCode MultiAcceptAsync(int64_t svr_fd, void* tag) = 0;

    /** @brief use `CloseAsync()` to close `svr_fd`. */
    virtual RetCode AcceptAsync(int64_t svr_fd, void* tag) = 0;

    /** @brief reads at most `sz` bytes into `buf` from `fd`. */
    virtual RetCode ReadAsync(int64_t fd, void* buf, uint64_t sz, void* tag) = 0;

    /** @brief writes at most `sz` bytes from `buf` to `fd`. */
    virtual RetCode WriteAsync(int64_t fd, const void* buf, uint64_t sz, void* tag) = 0;

    /** @brief closes `fd`. */
    virtual RetCode CloseAsync(int64_t fd, void* tag) = 0;

    /**
       @param `res` has different meanings according to events:
       - ACCEPTED: `res` is the client fd or -errno, `tag` is the value passed to `MultiAcceptAsync()`
       - READ: `res` is the number of bytes read or -errno, `tag` is the value passed to `ReadAsync()`.
       - WRITTEN: `res` is the number of bytes written or -errno, `tag` is the value passed to `WriteAsync()`.
       - CLOSED: `res` is the return value of `close()` or -errno, `tag` is the value passed to `CloseAsync()`.

       @note callers should handle failures according to `res`.
    */
    virtual RetCode Wait(int64_t* res, void** tag) = 0;
};

}

#endif
