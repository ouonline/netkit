#ifndef __NETKIT_TCP_CLIENT_H__
#define __NETKIT_TCP_CLIENT_H__

#include "notification_queue.h"
#include "netkit/connection_info.h"
#include "netkit/retcode.h"

namespace netkit {

class TcpClient {
public:
    virtual ~TcpClient() {}

    const ConnectionInfo& GetInfo() const {
        return m_info;
    }

    /**
       @brief only one reading operation can be outstanding at any given time. That is,
       callers MUST wait to receive a `tag` before calling `ReadAsync` again.
    */
    virtual RetCode ReadAsync(void* buf, uint64_t sz, void* tag, NotificationQueue*) = 0;

    /**
       @brief only one writing operation can be outstanding at any given time. That is,
       callers MUST wait to receive a `tag` before calling `WriteAsync` again.
    */
    virtual RetCode WriteAsync(const void* buf, uint64_t sz, void* tag, NotificationQueue*) = 0;

    virtual RetCode ShutDownAsync(void* tag, NotificationQueue*) = 0;

protected:
    ConnectionInfo m_info;
};

}

#endif
