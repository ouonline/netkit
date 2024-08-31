#ifndef __NETKIT_INTERNAL_STATE_H__
#define __NETKIT_INTERNAL_STATE_H__

namespace netkit {

struct State {
    enum Value {
        UNKNOWN,
        SERVER_ACCEPT,
        CLIENT_READ_REQ,
    } value;
    State(Value v = UNKNOWN) : value(v) {}
    virtual ~State() {}
};

}

#endif
