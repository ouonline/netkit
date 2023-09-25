#ifndef __NETKIT_EPOLL_READ_HANDLER_H__
#define __NETKIT_EPOLL_READ_HANDLER_H__

#include "event_handler.h"
#include <utility> // std::move()

namespace netkit { namespace epoll {

class ReadHandler final : public EventHandler {
public:
    ReadHandler() : m_buf(nullptr), m_sz(0), m_fd(-1) {}

    ReadHandler(ReadHandler&& h) {
        DoMove(std::move(h));
    }

    void operator=(ReadHandler&& h) {
        DoMove(std::move(h));
    }

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
    void DoMove(ReadHandler&& h) {
        m_buf = h.m_buf;
        m_sz = h.m_sz;
        m_fd = h.m_fd;
        h.m_buf = nullptr;
        h.m_sz = 0;
        h.m_fd = -1;
    }

private:
    void* m_buf;
    uint64_t m_sz;
    int m_fd;

private:
    ReadHandler(const ReadHandler&) = delete;
    void operator=(const ReadHandler&) = delete;
};

}}

#endif
