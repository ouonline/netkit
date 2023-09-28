#ifndef __NETKIT_EPOLL_NOTIFICATION_QUEUE_IMPL_H__
#define __NETKIT_EPOLL_NOTIFICATION_QUEUE_IMPL_H__

#include "netkit/notification_queue.h"
#include "logger/logger.h"
#include <sys/epoll.h>

namespace netkit { namespace epoll {

class NotificationQueueImpl final : public NotificationQueue {
public:
    NotificationQueueImpl() : m_epfd(-1), m_event_idx(0), m_nr_valid_event(0), m_logger(nullptr) {}
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
    static constexpr uint32_t MAX_EVENTS = 64;

private:
    int m_epfd;
    int m_event_idx;
    int m_nr_valid_event;
    struct epoll_event m_event_list[MAX_EVENTS];
    Logger* m_logger;

private:
    NotificationQueueImpl(const NotificationQueueImpl&) = delete;
    NotificationQueueImpl(NotificationQueueImpl&&) = delete;
    void operator=(const NotificationQueueImpl&) = delete;
    void operator=(NotificationQueueImpl&&) = delete;
};

}}

#endif
