#ifndef __NETKIT_EPOLL_READ_HANDLER_H__
#define __NETKIT_EPOLL_READ_HANDLER_H__

#include "event_handler.h"

namespace netkit { namespace epoll {

class ReadHandler final : public EventHandler {
public:
    void Init(int fd) {
        m_fd = fd;
    }

    void SetParameters(void* buf, uint64_t sz, void* _tag) {
        m_buf = buf;
        m_sz = sz;
        tag = _tag;
    }

    int64_t In() override;

private:
    void* m_buf;
    uint64_t m_sz;
    int m_fd;
};

}}

#endif
