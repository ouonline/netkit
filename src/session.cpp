#include "session.h"
#include <stdlib.h> // malloc() and free()
using namespace std;
using namespace threadkit;

namespace netkit {

Session* CreateSession(void) {
    auto ptr = (Session*)malloc(sizeof(Session) + sizeof(MPSCQueue::Node));
    if (!ptr) {
        return nullptr;
    }

    new (ptr) Session();
    new (GetNodeFromSession(ptr)) MPSCQueue::Node();

    return ptr;
}

void DestroySession(Session* session) {
    auto node = GetNodeFromSession(session);
    node->MPSCQueue::Node::~Node();
    session->~Session();
    free(session);
}

}
