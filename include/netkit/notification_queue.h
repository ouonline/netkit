#ifndef __NETKIT_NOTIFICATION_QUEUE_H__
#define __NETKIT_NOTIFICATION_QUEUE_H__

#include <stdint.h>
#include <time.h>

namespace netkit {

class NotificationQueue {
public:
    virtual ~NotificationQueue() {}

    /** @brief use `CloseAsync()` to close `svr_fd`. returns 0 or -errno. */
    virtual int MultiAcceptAsync(int64_t svr_fd, void* tag) = 0;

    /** @brief use `CloseAsync()` to close `svr_fd`. returns 0 or -errno. */
    virtual int AcceptAsync(int64_t svr_fd, void* tag) = 0;

    /** @brief receive at most `sz` bytes into `buf` from `fd`. returns 0 or -errno. */
    virtual int RecvAsync(int64_t fd, void* buf, uint64_t sz, void* tag) = 0;

    /** @brief send at most `sz` bytes from `buf` to `fd`. returns 0 or -errno. */
    virtual int SendAsync(int64_t fd, const void* buf, uint64_t sz, void* tag) = 0;

    /** @brief close `fd`. returns 0 or -errno. */
    virtual int CloseAsync(int64_t fd, void* tag) = 0;

    /**
       @brief gets next event. returns 0 or -errno.

       @param `res` has different meanings according to events:
       - ACCEPT: `res` is the client fd or -errno, `tag` is the value passed to `MultiAcceptAsync()` or `AcceptAsync()`.
       - RECV: `res` is the number of bytes read or -errno, `tag` is the value passed to `RecvAsync()`.
       - SEND: `res` is the number of bytes written or -errno, `tag` is the value passed to `SendAsync()`.
       - COPY: `res` is the number of bytes copied or -errno, `tag` is the value passed to `CopyAsync()`.
       - CLOSE: `res` is the return value of `close()` or -errno, `tag` is the value passed to `CloseAsync()`.

       @param timtout waits until at least one event arrives, timeout reaches, or some error occurs.
       nullptr for blocking without timeout.

       @return -EAGAIN if there is no events, 0 for success, and -errno for other errors.
    */
    virtual int Next(int64_t* res, void** tag, struct timeval* timeout) = 0;
};

}

#endif
