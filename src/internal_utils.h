#ifndef __NETKIT_INTERNAL_UTILS_H__
#define __NETKIT_INTERNAL_UTILS_H__

#include "netkit/nq_utils.h"
#include "netkit/buffer.h"
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
               `out`: data needed to be sent in this connection
             */
             const std::function<void(int32_t val, Buffer* out)>&,
             NotificationQueueImpl*, InternalClient*, Logger*);

}}

#endif
