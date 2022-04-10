#ifndef __NETKIT_EVENT_MANAGER_H__
#define __NETKIT_EVENT_MANAGER_H__

#include "status_code.h"
#include "event_handler.h"
#include "logger/logger.h"
#include <sys/epoll.h> // event definitions

namespace outils { namespace net {

class EventManager final {
public:
    EventManager(Logger* logger) : m_epfd(-1), m_logger(logger) {}
    StatusCode Init();
    StatusCode AddHandler(EventHandler* e, unsigned int event);
    StatusCode Loop();

private:
    int m_epfd;
    Logger* m_logger;

private:
    EventManager(const EventManager&);
    EventManager& operator=(const EventManager&);
};

}} // namespace outils::net

#endif
