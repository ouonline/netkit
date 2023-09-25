#include "netkit/epoll/eventfd_handler.h"
#include <unistd.h>
#include <errno.h>

namespace netkit { namespace epoll {

int64_t EventfdHandler::In() {
    int64_t unused = 0;
    auto nbytes = read(m_efd, &unused, sizeof(unused));
    if (nbytes == sizeof(unused)) {
        return m_in_res;
    }
    return -errno;
}

}}
