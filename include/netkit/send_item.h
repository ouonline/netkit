#ifndef __NETKIT_SEND_ITEM_H__
#define __NETKIT_SEND_ITEM_H__

#include "buffer.h"
#include <functional>

namespace netkit {

struct SendItem final {
    SendItem(Buffer&& b, const std::function<void(int err)>& f)
        : buffer(std::move(b)), on_complete(f) {}
    Buffer buffer;
    std::function<void(int err)> on_complete;
};

}

#endif
