#ifndef __NETKIT_REQUEST_HANDLER_H__
#define __NETKIT_REQUEST_HANDLER_H__

#include "buffer.h"
#include "sender.h"

namespace netkit {

enum ReqStat {
    // invalid request
    INVALID = -1,

    // ok, and `req_bytes` is set to the total size of the request.
    // note that `req_bytes` may be less than the size of buffer.
    VALID = 0,

    // more data required. `req_bytes` is ignored.
    MORE_DATA = 1,

    // more data required. `req_bytes` is set to bytes needed in current stage.
    MORE_DATA_WITH_SIZE = 2,
};

class RequestHandler {
public:
    virtual ~RequestHandler() {}
    virtual void OnConnected(const ConnectionInfo&) = 0;
    virtual void OnDisconnected(const ConnectionInfo&) = 0;
    virtual ReqStat Check(const Buffer&, uint64_t* req_bytes) = 0;
    virtual void Process(Buffer&&, Sender*) = 0;
};

}

#endif
