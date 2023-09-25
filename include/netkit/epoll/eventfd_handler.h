#ifndef __NETKIT_EPOLL_EVENTFD_HANDLER_H__
#define __NETKIT_EPOLL_EVENTFD_HANDLER_H__

#include "event_handler.h"
#include <utility> // std::move()

namespace netkit { namespace epoll {

class EventfdHandler final : public EventHandler {
public:
    EventfdHandler() : m_efd(-1), m_in_res(0) {}

    EventfdHandler(EventfdHandler&& h) {
        DoMove(std::move(h));
    }

    void operator=(EventfdHandler&& h) {
        DoMove(std::move(h));
    }

    void Init(int efd) {
        m_efd = efd;
    }
    void SetParameters(void* _tag, int64_t in_res) {
        tag = _tag;
        m_in_res = in_res;
    }

    int64_t In() override;

private:
    void DoMove(EventfdHandler&& h) {
        m_efd = h.m_efd;
        m_in_res = h.m_in_res;
        h.m_efd = -1;
        h.m_in_res = 0;
    }

private:
    int m_efd;
    int64_t m_in_res;

private:
    EventfdHandler(const EventfdHandler&) = delete;
    void operator=(const EventfdHandler&) = delete;
};

}}

#endif
