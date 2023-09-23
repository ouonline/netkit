#ifndef __NETKIT_IOURING_UTILS_H__
#define __NETKIT_IOURING_UTILS_H__

#include "netkit/retcode.h"
#include "liburing.h"
#include "logger/logger.h"
#include <functional>

namespace netkit { namespace iouring {

RetCode GenericAsync(struct io_uring*, Logger*, const std::function<void(struct io_uring_sqe*)>&);

}}

#endif
