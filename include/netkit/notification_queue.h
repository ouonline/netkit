#ifndef __NETKIT_NOTIFICATION_QUEUE_H__
#define __NETKIT_NOTIFICATION_QUEUE_H__

#include <stdint.h>

namespace netkit {

class NotificationQueue {
public:
    virtual ~NotificationQueue() {}

    /** @brief use `CloseAsync()` to close `svr_fd`. returns 0 or -errno. */
    virtual int MultiAcceptAsync(int64_t svr_fd, void* tag) = 0;

    /** @brief use `CloseAsync()` to close `svr_fd`. returns 0 or -errno. */
    virtual int AcceptAsync(int64_t svr_fd, void* tag) = 0;

    /** @brief reads at most `sz` bytes into `buf` from `fd`. returns 0 or -errno. */
    virtual int ReadAsync(int64_t fd, void* buf, uint64_t sz, void* tag) = 0;

    /** @brief writes at most `sz` bytes from `buf` to `fd`. returns 0 or -errno. */
    virtual int WriteAsync(int64_t fd, const void* buf, uint64_t sz, void* tag) = 0;

    /** @brief closes `fd`. returns 0 or -errno. */
    virtual int CloseAsync(int64_t fd, void* tag) = 0;

    /**
       @brief gets next event. returns 0 or -errno.

       @param `res` has different meanings according to events:
       - ACCEPTED: `res` is the client fd or -errno, `tag` is the value passed to `MultiAcceptAsync()`
       - READ: `res` is the number of bytes read or -errno, `tag` is the value passed to `ReadAsync()`.
       - WRITTEN: `res` is the number of bytes written or -errno, `tag` is the value passed to `WriteAsync()`.
       - CLOSED: `res` is the return value of `close()` or -errno, `tag` is the value passed to `CloseAsync()`.

       @param blocking block until at least one event arrives or error occurs. returns -EAGAIN if there is no events when `blocking` is false.

       @note callers should handle failures according to `res`.
    */
    virtual int Next(int64_t* res, void** tag, bool blocking = true) = 0;
};

}

#endif
