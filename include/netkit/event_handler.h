#ifndef __NETKIT_EVENT_HANDLER_H__
#define __NETKIT_EVENT_HANDLER_H__

#include "netkit/notification_queue.h"

namespace netkit {

class EventHandler {
public:
    // true to keep this instance, false otherwise
    virtual bool Process(int64_t, NotificationQueue*) = 0;

    virtual void DeleteSelf() {
        delete this;
    }

protected:
    virtual ~EventHandler() = default;
};

}

#endif
