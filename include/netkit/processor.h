#ifndef __NETKIT_PROCESSOR_H__
#define __NETKIT_PROCESSOR_H__

#include "buffer.h"
#include "connection.h"

namespace netkit {

class Processor {
public:
    /** returned values of CheckPacket() */
    enum PacketState {
        PACKET_INVALID = -1, /* invalid packet */
        PACKET_SUCCESS = 0, /* ok, and `packet_bytes` is set */
        PACKET_MORE_DATA = 1, /* more data required. `packet_bytes` is ignored */
    };

public:
    virtual ~Processor() {}

    virtual void OnConnected(Connection*) = 0;
    virtual void OnDisconnected(Connection*) = 0;

    virtual PacketState CheckPacket(Buffer*, uint64_t* packet_bytes) = 0;
    virtual bool ProcessPacket(Buffer*, Connection*) = 0;
};

} // namespace netkit

#endif
