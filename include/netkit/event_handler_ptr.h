#ifndef __NETKIT_EVENT_HANDLER_PTR_H__
#define __NETKIT_EVENT_HANDLER_PTR_H__

#include "event_handler.h"
#include <memory>

namespace netkit {

struct EventHandlerDeleter final {
    void operator()(EventHandler* h) const noexcept {
        h->DeleteSelf();
    }
};

template <typename T>
using EventHandlerPtr = std::unique_ptr<T, EventHandlerDeleter>;

}

#endif
