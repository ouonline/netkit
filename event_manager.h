#ifndef __NET_EVENT_MANAGER_H__
#define __NET_EVENT_MANAGER_H__

#include "status_code.h"
#include "event_handler.h"

namespace utils { namespace net {

class EventManager final {

public:
    EventManager() : m_epfd(-1) {}
    StatusCode Init();
    StatusCode AddServer(EventHandler* e);
    StatusCode AddClient(EventHandler* e);
    StatusCode Loop();

private:
    StatusCode SetNonBlocking(int fd);

private:
    int m_epfd;

private:
    EventManager(const EventManager&);
    EventManager& operator=(const EventManager&);
};

}}

#endif
