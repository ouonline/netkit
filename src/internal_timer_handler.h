#ifndef __NETKIT_INTERNAL_TIMER_HANDLER_H__
#define __NETKIT_INTERNAL_TIMER_HANDLER_H__

#include "state.h"
#include <functional>

namespace netkit {

struct InternalTimerHandler final : public State {
    InternalTimerHandler(int f, const std::function<void(int, uint64_t)>& h)
        : State(State::TIMER), fd(f), handler(h) {}
    ~InternalTimerHandler() {
        if (fd > 0) {
            close(fd);
        }
    }

    int fd;
    uint64_t nr_expiration = 0;
    std::function<void(int err, uint64_t nr_expiration)> handler;
};

}

#endif
