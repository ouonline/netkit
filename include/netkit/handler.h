#ifndef __NETKIT_HANDLER_H__
#define __NETKIT_HANDLER_H__

#include "buffer.h"
#include "connection_info.h"

namespace netkit {

enum ReqStat {
    /* invalid request */
    INVALID = -1,

    /*
      ok, and `req_bytes` is set to the total size of the request.
      note that `req_bytes` may be less than the size of buffer.
    */
    VALID = 0,

    /*
      more data required.

      `req_bytes` is set to the number of bytes left at the current stage,
      or is set to 0 if the number of bytes cannot be determined.
    */
    MORE_DATA = 1,
};

class Handler {
public:
    virtual ~Handler() {}
    virtual void OnConnected(const ConnectionInfo&, Buffer*) = 0;
    virtual void OnDisconnected() = 0;
    virtual ReqStat Check(const Buffer&, uint64_t* req_bytes) = 0;
    virtual void Process(Buffer&& req, Buffer* res) = 0;
};

}

#endif
