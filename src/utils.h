#ifndef __NETKIT_UTILS_H__
#define __NETKIT_UTILS_H__

#include "netkit/retcode.h"
#include <fcntl.h>
#include <cstring>

namespace netkit { namespace tcp {

inline RetCode SetNonBlocking(int fd, Logger* logger) {
    int opt;

    opt = fcntl(fd, F_GETFL);
    if (opt < 0) {
        logger_error(logger, "fcntl failed: %s.", strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }

    opt |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, opt) == -1) {
        logger_error(logger, "fcntl failed: %s.", strerror(errno));
        return RC_INTERNAL_NET_ERR;
    }

    return RC_SUCCESS;
}

}} // namespace netkit::tcp

#endif
