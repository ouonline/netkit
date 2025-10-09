#ifndef __NETKIT_INTERNAL_UTILS_H__
#define __NETKIT_INTERNAL_UTILS_H__

#include "netkit/nq_utils.h"
#include "internal_client.h"
#include <functional>

namespace netkit { namespace utils {

// returns -errno or 0
int InitThreadLocalNq(Logger*);

NotificationQueueImpl* GetThreadLocalNq();

// returns -errno or the timer fd
int AddTimer(const TimeVal& delay, const TimeVal& interval,
             /*
               `val` < 0: error occurs and `val` == -errno
               `val` > 0: the number of expirations
             */
             const std::function<int(int32_t val)>&, NotificationQueueImpl*,
             InternalClient*, Logger*);

}}

#endif
