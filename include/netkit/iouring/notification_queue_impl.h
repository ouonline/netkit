#ifndef __NETKIT_IOURING_NOTIFICATION_QUEUE_IMPL_H__
#define __NETKIT_IOURING_NOTIFICATION_QUEUE_IMPL_H__

#include "netkit/notification_queue.h"
#include "logger/logger.h"
#include "liburing.h"

namespace netkit { namespace iouring {

struct Locker;

struct NotificationQueueOptions final {
    /** @brief whether *Async() functions are thread-safe */
    bool thread_safe_async = false;
};

class NotificationQueueImpl final : public NotificationQueue {
public:
    NotificationQueueImpl() : m_locker(nullptr), m_logger(nullptr) {}
    ~NotificationQueueImpl() {
        Destroy();
    }

    RetCode Init(const NotificationQueueOptions&, Logger* l);

    /** @brief it is save to call `Destroy()` repeatly. */
    void Destroy();

    RetCode MultiAcceptAsync(int64_t fd, void* tag) override;
    RetCode ReadAsync(int64_t fd, void* buf, uint64_t sz, void* tag) override;
    RetCode WriteAsync(int64_t fd, const void* buf, uint64_t sz, void* tag) override;
    RetCode CloseAsync(int64_t fd, void* tag) override;

    /** @brief NOT thread-safe. */
    RetCode Wait(int64_t* res, void** tag) override;

private:
    struct io_uring m_ring;
    Locker* m_locker;
    Logger* m_logger;

private:
    NotificationQueueImpl(const NotificationQueueImpl&) = delete;
    NotificationQueueImpl(NotificationQueueImpl&&) = delete;
    void operator=(const NotificationQueueImpl&) = delete;
    void operator=(NotificationQueueImpl&&) = delete;
};

}}

#endif
