#ifndef __NETKIT_EVENT_QUEUE_H__
#define __NETKIT_EVENT_QUEUE_H__

#include "timeval.h"
#include "event_result.h"

namespace netkit {

// EventQueue is NOT thread-safe
class EventQueue {
public:
    virtual ~EventQueue() = default;

    /**
       @brief accepts one connection. returns 0 or -errno.
    */
    virtual int AcceptAsync(int fd, void* tag, bool multishot) = 0;

    /**
       @brief reads at most `sz` bytes into `buf` from `fd`. returns 0 or
       -errno.
    */
    virtual int ReadAsync(int fd, void* buf, uint64_t sz, void* tag) = 0;

    /**
       @brief writes at most `sz` bytes from `buf` to `fd`. returns 0 or -errno.
    */
    virtual int WriteAsync(int fd, const void* buf, uint64_t sz, void* tag) = 0;

    /**
       @brief closes `fd`. returns 0 or -errno.
    */
    virtual int CloseAsync(int fd, void* tag) = 0;

    /**
       @brief notifies another event queue about an event. returns 0 or
       -errno.

       if the notification cannot be delivered, an event with `tag` and
       `res` == -errno is generated in this queue instead. so `res` should not
       be negative if the receiver needs to tell them apart.
    */
    virtual int NotifyAsync(EventQueue* eq, int res, void* tag) = 0;

    /**
       @brief gets next event. returns 0 or -errno.

       @param `res` has different meanings according to events:
       - ACCEPT: client fd or -errno.
       - READ: number of bytes read or -errno.
       - WRITE: number of bytes written or -errno.
       - CLOSE: return value of `close()` or -errno.
       - NOTIFY: value passed to `NotifyAsync()`. or -errno in the sender's
         queue if the notification cannot be delivered.

       @param `tag` is the value passed to `*Async()`.

       @param `timtout` waits until at least one event arrives, timeout reaches,
       or some error occurs. `nullptr` for blocking without timeout.

       @return -EAGAIN if there is no events, 0 for success, and -errno for
       other errors.
    */
    virtual int Next(EventResult* res, void** tag, const TimeVal* timeout) = 0;
};

}

#endif
