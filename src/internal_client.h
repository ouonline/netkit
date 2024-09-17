#ifndef __NETKIT_INTERNAL_CLIENT_H__
#define __NETKIT_INTERNAL_CLIENT_H__

#include "netkit/handler.h"
#include "netkit/connection_info.h"
#include "threadkit/mpsc_queue.h"
#include "state.h"
#include "session.h"
#include <atomic>
#include <memory>

namespace netkit {

struct InternalClient final : public State {
    InternalClient(int fd, const std::shared_ptr<Handler>&);
    ~InternalClient();

    std::atomic<uint32_t> refcount;
    std::shared_ptr<Handler> handler;
    ConnectionInfo info;

    // `fd_for_reading` and `fd_for_writing` are identical.

    // ----- reading ----- //

    alignas(threadkit::CACHELINE_SIZE)

    const int fd_for_reading;
    uint64_t bytes_left;
    Buffer req;

    // ----- writing ----- //

    alignas(threadkit::CACHELINE_SIZE)

    const int fd_for_writing;
    uint64_t bytes_sent;
    Session* current_sending;
    threadkit::MPSCQueue res_queue;
};

inline InternalClient* CreateInternalClient(int fd, const std::shared_ptr<Handler>& h) {
    return new InternalClient(fd, h);
}

inline void DestroyInternalClient(InternalClient* c) {
    delete c;
}

inline void GetClient(InternalClient* c) {
    c->refcount.fetch_add(1, std::memory_order_acq_rel);
}

inline void PutClient(InternalClient* c) {
    auto prev = c->refcount.fetch_sub(1, std::memory_order_acq_rel);
    if (prev == 1) {
        delete c;
    }
}

}

#endif
