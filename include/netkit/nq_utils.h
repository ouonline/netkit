#ifndef __NETKIT_NQ_UTILS_H__
#define __NETKIT_NQ_UTILS_H__

#ifdef NETKIT_ENABLE_IOURING
#include "netkit/iouring/notification_queue_impl.h"
#elif defined(NETKIT_ENABLE_EPOLL)
#include "netkit/epoll/notification_queue_impl.h"
#else
#error "`NotificationQueueImpl` is not defined."
#endif

namespace netkit {

#ifdef NETKIT_ENABLE_IOURING

using netkit::iouring::NotificationQueueImpl;

inline int InitNq(NotificationQueueImpl* nq, Logger* l) {
    return nq->Init(netkit::iouring::NotificationQueueOptions(), l);
}

#elif defined(NETKIT_ENABLE_EPOLL)

using netkit::epoll::NotificationQueueImpl;

inline int InitNq(NotificationQueueImpl* nq, Logger* l) {
    return nq->Init(l);
}

#endif

}

#endif
