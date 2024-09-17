#ifndef __NETKIT_INTERNAL_TIMER_H__
#define __NETKIT_INTERNAL_TIMER_H__

#include "state.h"
#include <functional>

namespace netkit {

struct InternalTimer final : public State {
    InternalTimer(int f, const std::function<void(int32_t)>& h)
        : fd(f), handler(h) {}
    ~InternalTimer() {
        if (fd > 0) {
            close(fd);
        }
    }

    int fd;
    uint64_t nr_expiration = 0;
    std::function<void(int32_t val)> handler;
};

inline InternalTimer* CreateInternalTimer(int fd, const std::function<void(int32_t)>& h) {
    return new InternalTimer(fd, h);
}

inline void DestroyInternalTimer(InternalTimer* t) {
    delete t;
}

}

#endif
