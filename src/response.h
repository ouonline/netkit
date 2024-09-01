#ifndef __NETKIT_RESPONSE_H__
#define __NETKIT_RESPONSE_H__

#include "threadkit/mpsc_queue.h"

namespace netkit {

struct InternalClient;

struct Response final : public threadkit::MPSCQueue::Node {
    Buffer data;
    InternalClient* client;
    std::function<void(int)> callback;
};

}

#endif
