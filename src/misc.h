#ifndef __NETKIT_MISC_H__
#define __NETKIT_MISC_H__

#include <errno.h>

inline bool ShouldRetry(int err) {
    return (err == -EAGAIN || err == -EINTR);
}

#endif
