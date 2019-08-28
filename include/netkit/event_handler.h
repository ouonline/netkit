#ifndef __NET_EVENT_HANDLER_H__
#define __NET_EVENT_HANDLER_H__

#include "status_code.h"

namespace outils { namespace net {

class EventHandler {
public:
    virtual ~EventHandler() {}
    virtual int GetFd() const = 0;
    virtual StatusCode In() = 0;
    virtual StatusCode Out() = 0;
    virtual void Error() = 0;
};

}} // namespace outils::net

#endif
