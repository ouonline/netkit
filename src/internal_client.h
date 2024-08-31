#ifndef __NETKIT_INTERNAL_CLIENT_H__
#define __NETKIT_INTERNAL_CLIENT_H__

#include "netkit/request_handler.h"
#include "netkit/utils.h"
#include "threadkit/mpsc_queue.h"
#include "response.h"
#include "state.h"
#include <memory>
#include <unistd.h> // close()

namespace netkit {

struct InternalClient final : public State {
    InternalClient(int _fd, const std::shared_ptr<RequestHandler>& h)
        : fd(_fd), handler(h), bytes_needed(0), bytes_sent(0)
        , current_sending_res(nullptr) {
        utils::GenConnectionInfo(fd, &info);
        refcount.store(0, std::memory_order_relaxed);
        handler->OnConnected(info);
    }

    ~InternalClient() {
        if (fd > 0) {
            close(fd);
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

    int fd;
    std::shared_ptr<RequestHandler> handler;

    ConnectionInfo info;
    std::atomic<uint32_t> refcount;

    // ----- reading ----- //

    uint64_t bytes_needed;
    Buffer req;

    // ----- writing ----- //

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
