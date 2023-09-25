#ifndef __NETKIT_NOTIFICATION_QUEUE_H__
#define __NETKIT_NOTIFICATION_QUEUE_H__

#include "netkit/retcode.h"
#include <stdint.h>

namespace netkit {

class NotificationQueue {
public:
    virtual ~NotificationQueue() {}

    /**
       @param `res` has different meanings according to events:
       - CONNECTED: `res` is the client fd or -errno, `tag` is the value passed to `TcpServer::MultiAcceptAsync()`
       - WRITE: `res` is the number of bytes sent or -errno, `tag` is the value passed to `TcpClient::WriteAsync()`.
       - READ: `res` is the number of bytes received or -errno, `tag` is the value passed to `TcpClient::ReadAsync()`.
       - SHUTDOWN: `res` is the return value of `shutdown()`, `tag` is the value passed to `TcpClient::ShutDownAsync()`.

       @note note that callers should handle failures when `res` is negative.
    */
    virtual RetCode Wait(int64_t* res, void** tag) = 0;
};

}

#endif
