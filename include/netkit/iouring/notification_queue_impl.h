#ifndef __NETKIT_IOURING_NOTIFICATION_QUEUE_IMPL_H__
#define __NETKIT_IOURING_NOTIFICATION_QUEUE_IMPL_H__

#include "netkit/notification_queue.h"
#include "logger/logger.h"
#include "liburing.h"

namespace netkit { namespace iouring {

struct NotificationQueueOptions final {
    /** @brief creates a kernel thread to poll the SQ ring */
    bool enable_kernel_polling = false;
};

class NotificationQueueImpl final : public NotificationQueue {
public:
    NotificationQueueImpl() : m_logger(nullptr) {}
    ~NotificationQueueImpl() {
        Destroy();
    }

    int Init(const NotificationQueueOptions&, Logger* l);
    void Destroy(); // destroy this instance if necessary

    int MultiAcceptAsync(int64_t fd, void* tag) override;
    int AcceptAsync(int64_t svr_fd, void* tag) override;
    int ReadAsync(int64_t fd, void* buf, uint64_t sz, void* tag) override;
    int WriteAsync(int64_t fd, const void* buf, uint64_t sz, void* tag) override;
    int CloseAsync(int64_t fd, void* tag) override;

    int Next(int64_t* res, void** tag, bool blocking = true) override;

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
