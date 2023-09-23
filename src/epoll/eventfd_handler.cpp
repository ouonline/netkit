#include "netkit/epoll/eventfd_handler.h"
#include <unistd.h>
#include <errno.h>

namespace netkit { namespace epoll {

int64_t EventfdHandler::In() {
    int64_t res = 0;
    auto nbytes = read(m_efd, &res, sizeof(res));
    if (nbytes == sizeof(res)) {
        return res;
    }
    return -errno;
}

}}
