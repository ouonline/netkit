#ifndef __NETKIT_ENDPOINT_INFO_H__
#define __NETKIT_ENDPOINT_INFO_H__

#include <stdint.h>
#include <string>

namespace netkit {

struct EndpointInfo final {
    EndpointInfo() {}
    EndpointInfo(const EndpointInfo&) = default;
    EndpointInfo& operator=(const EndpointInfo&) = default;

    EndpointInfo(EndpointInfo&& info) {
        DoMove(std::move(info));
    }
    EndpointInfo& operator=(EndpointInfo&& info) {
        DoMove(std::move(info));
        return *this;
    }

    void DoMove(EndpointInfo&& info) {
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
