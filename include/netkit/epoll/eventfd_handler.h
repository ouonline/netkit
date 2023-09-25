#ifndef __NETKIT_EPOLL_EVENTFD_HANDLER_H__
#define __NETKIT_EPOLL_EVENTFD_HANDLER_H__

#include "event_handler.h"

namespace netkit { namespace epoll {

class EventfdHandler final : public EventHandler {
public:
    void Init(int efd) {
        m_efd = efd;
    }
    void SetParameters(void* _tag, int64_t in_res) {
        tag = _tag;
        m_in_res = in_res;
    }

    int64_t In() override;

private:
    int m_efd;
    int64_t m_in_res;
};

}}

#endif
