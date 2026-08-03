#ifndef __NETKIT_TIMER_H__
#define __NETKIT_TIMER_H__

#include "event_handler_ptr.h"
#include "connection.h"
#include "logger/logger.h"
#include <memory>

namespace netkit {

class SendContext;

class Timer : public EventHandler {
protected:
    Timer(Logger* l) : m_logger(l) {}
    virtual ~Timer();

    /*
      `val` < 0: error occurs and `val` == -errno
      `val` > 0: the number of expirations
      returns true to keep this timer
    */
    virtual bool OnExpiration(int val, SendContext*) = 0;

protected:
    Logger* m_logger;

private:
    friend class SendContext;

    int Init(int fd, const std::shared_ptr<Connection>&);
    int Start(NotificationQueue*);
    bool Process(EventResult, NotificationQueue*) final;

private:
    int m_fd;
    uint64_t m_nr_expiration;
    std::shared_ptr<Connection> m_conn;
};

using TimerPtr = EventHandlerPtr<Timer>;

}

#endif
