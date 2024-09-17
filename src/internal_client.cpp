#include "internal_client.h"
#include "netkit/utils.h"
#include <unistd.h> // close()
using namespace std;

namespace netkit {

InternalClient::InternalClient(int fd, const shared_ptr<Handler>& h)
    : refcount(0), handler(h)
    , fd_for_reading(fd), bytes_left(0)
    , fd_for_writing(fd), bytes_sent(0), current_sending(nullptr) {
    utils::GenConnectionInfo(fd, &info);
}

InternalClient::~InternalClient() {
    if (fd_for_writing > 0) {
        close(fd_for_writing);
    }

    delete current_sending;
    while (true) {
        auto node = res_queue.PopNode();
        if (!node) {
            return;
        }
        auto session = GetSessionFromNode(node);
        DestroySession(session);
    }

    handler->OnDisconnected();
}

}
