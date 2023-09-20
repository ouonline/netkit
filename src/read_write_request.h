#ifndef __NETKIT_READ_WRITE_REQUEST_H__
#define __NETKIT_READ_WRITE_REQUEST_H__

#include "request.h"
#include "netkit/connection.h"
#include <functional>

namespace netkit {

struct ReadWriteRequest final : public Request {
    ReadWriteRequest(const std::function<void(uint64_t)>& cb, Request::Type t) : Request(t), callback(cb) {}
    std::function<void(uint64_t)> callback;
};

}

#endif
