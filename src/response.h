#ifndef __NETKIT_RESPONSE_H__
#define __NETKIT_RESPONSE_H__

#include "internal_client.h"
#include "threadkit/mpsc_queue.h"

namespace netkit {

struct Response final : public threadkit::MPSCQueue::Node {
    Response() {
        qbuf_init(&data);
    }
    ~Response() {
        qbuf_destroy(&data);
    }

    QBuf data;
    InternalClient* client;
    std::function<void(int)> callback;
};

}

#endif
