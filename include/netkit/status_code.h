#ifndef __NET_STATUS_CODE_H__
#define __NET_STATUS_CODE_H__

#include <stdint.h>

namespace outils { namespace net {

enum {
    SC_OK = 0,
    SC_NOMEM,
    SC_CLIENT_DISCONNECTED,
    SC_INTERNAL_NET_ERR,
    SC_REQ_PACKET_ERR,
};

typedef uint32_t StatusCode;

}} // namespace outils::net

#endif
