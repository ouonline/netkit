#ifndef __NETKIT_INTERNAL_TIMER_H__
#define __NETKIT_INTERNAL_TIMER_H__

#include "state.h"
#include <functional>

namespace netkit {

struct InternalTimer final : public State {
    InternalTimer(int _fd, InternalClient* c,
                  const std::function<void(int32_t, Buffer*)>& cb)
        : fd(_fd), sent_errno(0), client(c), callback(cb) {}
    ~InternalTimer() {
        if (fd > 0) {
            close(fd);
        }
    }

    int fd;
    int32_t sent_errno;
    uint64_t nr_expiration = 0;
    InternalClient* client;
    std::function<void(int32_t val, Buffer* out)> callback;
};

inline InternalTimer* CreateInternalTimer(
    int fd, InternalClient* c, const std::function<void(int32_t, Buffer*)>& cb) {
    return new InternalTimer(fd, c, cb);
}

inline void DestroyInternalTimer(InternalTimer* t) {
    delete t;
}

}

#endif
