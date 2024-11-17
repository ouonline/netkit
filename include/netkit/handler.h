#ifndef __NETKIT_HANDLER_H__
#define __NETKIT_HANDLER_H__

#include "buffer.h"
#include "connection.h"
#include <functional>

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
    /* `Connection` can be used during lifetime */
    virtual int OnConnected(Connection*) = 0;
    virtual void OnDisconnected() = 0;
    virtual ReqStat Check(const Buffer&, uint64_t* req_bytes) = 0;
    virtual void Process(Buffer&& req) = 0;
};

}

#endif
