#ifndef __NET_EVENT_MANAGER_H__
#define __NET_EVENT_MANAGER_H__

#include "status_code.h"
#include "event_handler.h"
#include "logger/logger.h"

namespace utils { namespace net {

class EventManager final {

public:
    EventManager(struct logger* logger) :m_epfd(-1), m_logger(logger) {}
    StatusCode Init();
    StatusCode AddServer(EventHandler* e);
    StatusCode AddClient(EventHandler* e);
    StatusCode Loop();

private:
    StatusCode SetNonBlocking(int fd);

private:
    int m_epfd;
    struct logger* m_logger;

private:
    EventManager(const EventManager&);
    EventManager& operator=(const EventManager&);
};

}}

#endif
