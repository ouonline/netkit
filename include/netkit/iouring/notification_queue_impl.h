#ifndef __NETKIT_IOURING_NOTIFICATION_QUEUE_IMPL_H__
#define __NETKIT_IOURING_NOTIFICATION_QUEUE_IMPL_H__

#include "netkit/notification_queue.h"
#include "logger/logger.h"
#include "liburing.h"

namespace netkit { namespace iouring {

class NotificationQueueImpl final : public NotificationQueue {
public:
    struct Options final {
        /** @brief max number of notifications in queue */
        uint32_t queue_size = 1024;
    };

public:
    NotificationQueueImpl() : m_logger(nullptr) {}
    ~NotificationQueueImpl() {
        Destroy();
    }

    int Init(const Options&, Logger* l);
    void Destroy(); // destroy this instance if necessary

    int AcceptAsync(uintptr_t svr_fd, void* tag, bool multishot) override;
    int ReadAsync(uintptr_t fd, void* buf, uint64_t sz, void* tag) override;
    int WriteAsync(uintptr_t fd, const void* buf, uint64_t sz,
                   void* tag) override;
    int CloseAsync(uintptr_t fd, void* tag) override;
    int NotifyAsync(NotificationQueue*, int res, void* tag) override;

    int Next(EventResult* res, void** tag, const TimeVal* timeout) override;

private:
    struct io_uring m_ring;
    Logger* m_logger;

private:
    NotificationQueueImpl(const NotificationQueueImpl&) = delete;
    NotificationQueueImpl(NotificationQueueImpl&&) = delete;
    void operator=(const NotificationQueueImpl&) = delete;
    void operator=(NotificationQueueImpl&&) = delete;
};

}}

#endif
