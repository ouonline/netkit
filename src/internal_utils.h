#ifndef __NETKIT_INTERNAL_UTILS_H__
#define __NETKIT_INTERNAL_UTILS_H__

#include "netkit/notification_queue.h"
#include "netkit/scheduler.h"
#include "logger/logger.h"
#include <memory>
#include <functional>

namespace netkit { namespace utils {

int DoAddTimer(const TimeVal& delay, const TimeVal& interval,
               /*
                 `val` < 0: error occurs and `val` == -errno
                 `val` > 0: the number of expirations
               */
               const std::function<void(int32_t val)>&, NotificationQueue*,
               Logger*);

}}

#endif
