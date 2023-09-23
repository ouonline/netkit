#include "netkit/epoll/read_handler.h"
#include <unistd.h>
#include <errno.h>

namespace netkit { namespace epoll {

int64_t ReadHandler::In() {
    auto nbytes = read(m_fd, m_buf, m_sz);
    if (nbytes == -1) {
        return -errno;
    }
    return nbytes;
}

}}
