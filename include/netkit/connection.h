#ifndef __NETKIT_CONNECTION_H__
#define __NETKIT_CONNECTION_H__

#include "send_item.h"
#include "endpoint_info.h"
#include "logger/logger.h"
#include <set>
#include <atomic>
#include <mutex>
#include <queue>

namespace netkit {

class Connection final {
private:
    std::atomic<uint32_t> m_is_valid = {1};
    EndpointInfo m_info;

public:
    Connection(int _fd) : fd(_fd) {}
    ~Connection();

    const EndpointInfo& GetEndpointInfo();
    void ShutDown(Logger*);

    bool IsValid() const {
        return m_is_valid.load(std::memory_order_relaxed);
    }

    const int fd;
    std::mutex timer_lock;
    std::set<int> timer_fds;
    std::mutex send_lock;
    uint32_t send_offset = 0;
    std::queue<SendItem> send_queue;
};

}

#endif
