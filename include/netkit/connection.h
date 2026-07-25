#ifndef __NETKIT_CONNECTION_H__
#define __NETKIT_CONNECTION_H__

#include "send_item.h"
#include "endpoint_info.h"
#include "logger/logger.h"
#include <mutex>
#include <queue>

namespace netkit {

class Connection final {
private:
    EndpointInfo m_info;

public:
    Connection(int _fd, Logger* l) : fd(_fd), logger(l) {}
    ~Connection();

    const EndpointInfo& GetEndpointInfo();

    int fd;
    Logger* logger;
    std::mutex lock;
    uint32_t sending_offset = 0;
    std::queue<SendItem> send_queue;
};

}

#endif
