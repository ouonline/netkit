#ifndef __NETKIT_RETCODE_H__
#define __NETKIT_RETCODE_H__

#include <stdint.h>

namespace netkit {

enum {
    RC_OK = 0,
    RC_INTERNAL_NET_ERR,
};

typedef uint32_t RetCode;

}

#endif
