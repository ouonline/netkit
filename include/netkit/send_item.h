#ifndef __NETKIT_SEND_ITEM_H__
#define __NETKIT_SEND_ITEM_H__

#include "buffer.h"
#include <functional>

namespace netkit {

struct SendItem final {
    SendItem(Buffer&& b, const std::function<void(int err)>& f)
        : data(std::move(b)), on_sent(f) {}
    Buffer data;
    std::function<void(int err)> on_sent;
};

}

#endif
