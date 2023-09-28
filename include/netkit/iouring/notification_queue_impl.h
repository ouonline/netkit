#ifndef __NETKIT_IOURING_NOTIFICATION_QUEUE_IMPL_H__
#define __NETKIT_IOURING_NOTIFICATION_QUEUE_IMPL_H__

#include "netkit/notification_queue.h"
#include "netkit/retcode.h"
#include "logger/logger.h"
#include "liburing.h"
#include <functional>

namespace netkit { namespace iouring {

class NotificationQueueImpl final : public NotificationQueue {
public:
    NotificationQueueImpl() : m_logger(nullptr) {}
    ~NotificationQueueImpl() {
        Destroy();
    }

    RetCode Init(Logger* l);
    void Destroy();

    RetCode MultiAcceptAsync(int64_t fd, void* tag) override;
    RetCode ReadAsync(int64_t fd, void* buf, uint64_t sz, void* tag) override;
    RetCode WriteAsync(int64_t fd, const void* buf, uint64_t sz, void* tag) override;
    RetCode Wait(int64_t* res, void** tag) override;

private:
    RetCode GenericAsync(const std::function<void(struct io_uring_sqe*)>&);

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
