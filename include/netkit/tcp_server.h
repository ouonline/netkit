#ifndef __NETKIT_TCP_SERVER_H__
#define __NETKIT_TCP_SERVER_H__

#include "notification_queue.h"
#include "retcode.h"

namespace netkit {

class TcpServer {
public:
    virtual ~TcpServer() {}
    virtual RetCode MultiAcceptAsync(void* tag, NotificationQueue*) = 0;
};

}

#endif
