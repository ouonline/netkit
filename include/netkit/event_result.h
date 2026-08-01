#ifndef __NETKIT_EVENT_RESULT_H__
#define __NETKIT_EVENT_RESULT_H__

#include <stdint.h>

namespace netkit {

struct EventResult final {
    uintptr_t val;
    int32_t err;
};

}

#endif
