#ifndef __NETKIT_EPOLL_EVENT_HANDLER_H__
#define __NETKIT_EPOLL_EVENT_HANDLER_H__

#include <stdint.h>

namespace netkit { namespace epoll {

struct EventHandler {
    EventHandler(void* t = nullptr) : tag(t) {}
    virtual ~EventHandler() {}
    virtual int64_t In() = 0;

    void* tag;
};

}}

#endif
