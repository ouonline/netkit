#ifndef __NETKIT_INTERNAL_TIMER_H__
#define __NETKIT_INTERNAL_TIMER_H__

#include "netkit/event_handler.h"
#include "logger/logger.h"
#include <functional>

namespace netkit {

class InternalTimer final : public EventHandler {
public:
    InternalTimer(int fd, const std::function<void(int val)>& cb, Logger* l)
        : m_fd(fd), m_cb(cb), m_logger(l) {}

    int Start(NotificationQueue*);
    bool Process(int64_t, NotificationQueue*) override;

protected:
    ~InternalTimer();

private:
    int m_fd;
    uint64_t m_nr_expiration;
    std::function<void(int val)> m_cb;
    Logger* m_logger;
};

}

#endif
