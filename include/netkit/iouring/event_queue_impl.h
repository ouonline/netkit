#ifndef __NETKIT_IOURING_EVENT_QUEUE_IMPL_H__
#define __NETKIT_IOURING_EVENT_QUEUE_IMPL_H__

#include "netkit/event_queue.h"
#include "logger/logger.h"
#include "liburing.h"

namespace netkit { namespace iouring {

class EventQueueImpl final : public EventQueue {
public:
    struct Options final {
        /** @brief max number of notifications in queue */
        uint32_t queue_size = 1024;
    };

public:
    EventQueueImpl() : m_logger(nullptr) {}
    ~EventQueueImpl() {
        Destroy();
    }

    int Init(const Options&, Logger* l);
    void Destroy(); // destroy this instance if necessary

    int AcceptAsync(int fd, void* tag, bool multishot) override;
    int ReadAsync(int fd, void* buf, uint64_t sz, void* tag) override;
    int WriteAsync(int fd, const void* buf, uint64_t sz, void* tag) override;
    int CloseAsync(int fd, void* tag) override;
    int NotifyAsync(EventQueue*, int res, void* tag) override;

    int Next(EventResult* res, void** tag, const TimeVal* timeout) override;

private:
    struct io_uring m_ring;
    Logger* m_logger;

private:
    EventQueueImpl(const EventQueueImpl&) = delete;
    EventQueueImpl(EventQueueImpl&&) = delete;
    void operator=(const EventQueueImpl&) = delete;
    void operator=(EventQueueImpl&&) = delete;
};

}}

#endif
