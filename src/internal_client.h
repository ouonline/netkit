#ifndef __NETKIT_INTERNAL_CLIENT_H__
#define __NETKIT_INTERNAL_CLIENT_H__

#include "netkit/handler.h"
#include "netkit/connection.h"
#include "threadkit/common.h"
#include "state.h"
#include "session.h"
#include <atomic>
#include <memory>
#include <queue>

namespace netkit {

struct InternalClient final : public State {
    InternalClient(int fd, const std::shared_ptr<Handler>& h,
                   NotificationQueueImpl* new_rd_nq,
                   NotificationQueueImpl* wr_nq, Logger* l)
        : refcount(0), handler(h), conn(fd, new_rd_nq, wr_nq, this, l)
        , fd_for_reading(fd), bytes_left(0)
        , fd_for_writing(fd), bytes_sent(0), current_sending(nullptr) {}
    ~InternalClient();

    std::atomic<uint32_t> refcount;
    std::shared_ptr<Handler> handler;
    Connection conn;

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
    std::queue<Session*> send_queue;
};

inline InternalClient* CreateInternalClient(int fd, const std::shared_ptr<Handler>& h,
                                            NotificationQueueImpl* new_rd_nq,
                                            NotificationQueueImpl* wr_nq, Logger* l) {
    return new InternalClient(fd, h, new_rd_nq, wr_nq, l);
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
        DestroyInternalClient(c);
    }
}

}

#endif
