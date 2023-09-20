#ifndef __NETKIT_SHUTDOWN_REQUEST_H__
#define __NETKIT_SHUTDOWN_REQUEST_H__

#include "request.h"
#include <functional>

namespace netkit {

struct ShutDownRequest final : public Request {
    ShutDownRequest(const std::function<void()>& cb) : Request(Request::SHUTDOWN), callback(cb) {}
    std::function<void()> callback;
};

}

#endif
