#ifndef __NETKIT_SESSION_H__
#define __NETKIT_SESSION_H__

#include "state.h"
#include "netkit/buffer.h"
#include "threadkit/mpsc_queue.h"

namespace netkit {

struct InternalClient;

// NOTE: we place a MPSCQueue::Node after this struct so as to avoid multiple inheritance
struct Session final : public State {
    Buffer data;
    InternalClient* client;
};

Session* CreateSession(void);
void DestroySession(Session*);

inline threadkit::MPSCQueue::Node* GetNodeFromSession(Session* session) {
    return (threadkit::MPSCQueue::Node*)((uint64_t)session + sizeof(Session));
}

inline Session* GetSessionFromNode(threadkit::MPSCQueue::Node* node) {
    return (Session*)((uint64_t)node - sizeof(Session));
}

}

#endif
