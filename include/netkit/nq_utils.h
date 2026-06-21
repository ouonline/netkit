#ifndef __NETKIT_NQ_UTILS_H__
#define __NETKIT_NQ_UTILS_H__

#include "netkit/iouring/notification_queue_impl.h"

namespace netkit {

using netkit::iouring::NotificationQueueImpl;

inline int InitNq(NotificationQueueImpl* nq, Logger* l) {
    return nq->Init(netkit::iouring::NotificationQueueOptions(), l);
}

}

#endif
