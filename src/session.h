#ifndef __NETKIT_SESSION_H__
#define __NETKIT_SESSION_H__

#include "state.h"
#include "netkit/buffer.h"
#include <functional>

namespace netkit {

struct InternalClient;

struct Session final : public State {
    Buffer data;
    InternalClient* client;
    std::function<void(int err)> sent_callback;
};

inline Session* CreateSession(void) {
    return new Session();
}

inline void DestroySession(Session* s) {
    delete s;
}

}

#endif
