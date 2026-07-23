#ifndef __NETKIT_CONNECTION_H__
#define __NETKIT_CONNECTION_H__

#include "utils.h"
#include "send_item.h"
#include "endpoint_info.h"
#include "logger/logger.h"
#include <mutex>
#include <queue>

namespace netkit {

class Connection final {
private:
    EndpointInfo info;

public:
    Connection(int _fd, Logger* l) : fd(_fd), logger(l) {}

    const EndpointInfo& GetEndpointInfo() {
        if (info.remote_port == 0) {
            std::lock_guard<std::mutex> _l(lock);
            if (info.remote_port == 0) {
                utils::GenEndpointInfo(fd, &info);
            }
        }
        return info;
    }

    int fd;
    Logger* logger;
    std::mutex lock;
    uint32_t sending_offset = 0;
    std::queue<SendItem> send_queue;
};

}

#endif
