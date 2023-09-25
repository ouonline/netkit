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
    RetCode Wait(int64_t* res, void** tag) override;

private:
    friend class TcpServerImpl;
    friend class TcpClientImpl;
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
