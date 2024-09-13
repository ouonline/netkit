#ifndef __NETKIT_INTERNAL_CLIENT_H__
#define __NETKIT_INTERNAL_CLIENT_H__

#include "netkit/handler.h"
#include "netkit/utils.h"
#include "threadkit/mpsc_queue.h"
#include "response.h"
#include "state.h"
#include <memory>
#include <unistd.h> // close()

namespace netkit {

struct InternalClient final : public State {
    InternalClient(int _fd, const std::shared_ptr<Handler>& h)
        : refcount(0), handler(h)
        , fd_for_reading(_fd), bytes_left(0)
        , fd_for_writing(_fd), bytes_sent(0), current_sending_res(nullptr) {
        utils::GenConnectionInfo(_fd, &info);
    }

    ~InternalClient() {
        if (fd_for_reading > 0) {
            close(fd_for_reading);
        }

        handler->OnDisconnected(info);

        delete current_sending_res;
        while (true) {
            auto node = res_queue.PopNode();
            if (!node) {
                return;
            }
            delete static_cast<Response*>(node);
        }
    }

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
    Response* current_sending_res;
    threadkit::MPSCQueue res_queue;
};

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
