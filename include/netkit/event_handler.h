#ifndef __NETKIT_EVENT_HANDLER_H__
#define __NETKIT_EVENT_HANDLER_H__

#include "retcode.h"

namespace netkit {

class EventHandler {
public:
    virtual ~EventHandler() {}
    virtual int GetFd() const = 0;
    virtual RetCode In() = 0;
    virtual RetCode Out() = 0;
    virtual void ShutDown() = 0;
};

} // namespace netkit

#endif
