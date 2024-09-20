#include "internal_client.h"
#include <unistd.h> // close()
using namespace std;

namespace netkit {

InternalClient::~InternalClient() {
    if (fd_for_writing > 0) {
        close(fd_for_writing);
    }

    delete current_sending;

    while (!send_queue.empty()) {
        auto session = send_queue.front();
        send_queue.pop();
        DestroySession(session);
    }

    handler->OnDisconnected();
}

}
