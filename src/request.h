#ifndef __NETKIT_REQUEST_H__
#define __NETKIT_REQUEST_H__

namespace netkit {

struct Request {
    enum Type {
        UNKNOWN,
        ACCEPT,
        READ,
        WRITE,
        SHUTDOWN,
    } type;
    Request(Type t = UNKNOWN) : type(t) {}
};

}

#endif
