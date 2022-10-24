#ifndef __NETKIT_STATUS_CODE_H__
#define __NETKIT_STATUS_CODE_H__

#include <stdint.h>

namespace netkit {

enum {
    RC_SUCCESS = 0,
    RC_NOMEM,
    RC_CLIENT_DISCONNECTED,
    RC_INTERNAL_NET_ERR,
    RC_REQ_PACKET_ERR,
    RC_TIMEOUT,
};

typedef uint32_t RetCode;

} // namespace netkit

#endif
