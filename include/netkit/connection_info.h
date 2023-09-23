#ifndef __NETKIT_CONNECTION_INFO_H__
#define __NETKIT_CONNECTION_INFO_H__

#include <stdint.h>
#include <string>

namespace netkit {

struct ConnectionInfo final {
    ConnectionInfo() {}
    ConnectionInfo(const ConnectionInfo&) = default;
    ConnectionInfo& operator=(const ConnectionInfo&) = default;

    ConnectionInfo(ConnectionInfo&& info) {
        DoMove(std::move(info));
    }
    ConnectionInfo& operator=(ConnectionInfo&& info) {
        DoMove(std::move(info));
        return *this;
    }

    void DoMove(ConnectionInfo&& info) {
        local_port = info.local_port;
        remote_port = info.remote_port;
        local_addr = std::move(info.local_addr);
        remote_addr = std::move(info.remote_addr);
        info.local_port = 0;
        info.remote_port = 0;
    }

    uint16_t local_port = 0;
    uint16_t remote_port = 0;
    std::string local_addr;
    std::string remote_addr;
};

}

#endif
