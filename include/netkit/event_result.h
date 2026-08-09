#ifndef __NETKIT_EVENT_RESULT_H__
#define __NETKIT_EVENT_RESULT_H__

#include <stdint.h>

namespace netkit {

struct EventResult final {
    int32_t val; // value or -errno
};

}

#endif
