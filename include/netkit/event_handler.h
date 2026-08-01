#ifndef __NETKIT_EVENT_HANDLER_H__
#define __NETKIT_EVENT_HANDLER_H__

#include "notification_queue.h"
#include "event_result.h"

namespace netkit {

class EventHandler {
public:
    // true to keep this instance, false otherwise
    virtual bool Process(EventResult, NotificationQueue*) = 0;

    virtual void DeleteSelf() {
        delete this;
    }

protected:
    virtual ~EventHandler() = default;
};

}

#endif
