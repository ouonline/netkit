#ifndef __NETKIT_EVENT_MANAGER_H__
#define __NETKIT_EVENT_MANAGER_H__

#include "retcode.h"
#include "event_handler.h"
#include "logger/logger.h"
#include <sys/epoll.h> // event definitions

namespace netkit {

class EventManager final {
public:
    EventManager(Logger* logger) : m_epfd(-1), m_logger(logger) {}
    RetCode Init();
    RetCode AddHandler(EventHandler* e, unsigned int event);
    RetCode Loop();

private:
    int m_epfd;
    Logger* m_logger;

private:
    EventManager(const EventManager&);
    EventManager& operator=(const EventManager&);
};

} // namespace netkit

#endif
