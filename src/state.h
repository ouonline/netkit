#ifndef __NETKIT_STATE_H__
#define __NETKIT_STATE_H__

namespace netkit {

struct State {
    enum {
        UNKNOWN,
        SERVER_ACCEPT,
        CLIENT_READ_REQ,
        WORKER_PROCESS_REQ,
        TIMER_EXPIRED,
        TIMER_NEXT,
    };
    int value = UNKNOWN;
};

}

#endif
