#ifndef __NETKIT_PROCESSOR_H__
#define __NETKIT_PROCESSOR_H__

#include "buffer.h"
#include "connection.h"
#include "threadkit/threadpool.h"

namespace netkit {

class Processor : public threadkit::ThreadTask {
public:
public:
    Processor() : m_conn(nullptr) {}
    virtual ~Processor() {}

    /** returned values of CheckPacket() */
    enum PacketState {
        PACKET_INVALID = -1, /* invalid packet */
        PACKET_SUCCESS = 0, /* ok, and `packet_bytes` is set */
        PACKET_MORE_DATA = 1, /* more data required. `packet_bytes` is ignored */
    };

    virtual PacketState CheckPacket(uint64_t* packet_bytes) = 0;

    Buffer* GetPacket() {
        return &m_buf;
    }
    void SetConnection(Connection* c) {
        m_conn = c;
    }

    virtual void OnConnected(Connection*) = 0;
    virtual void OnDisconnected(Connection*) = 0;

protected:
    virtual bool ProcessPacket(Connection*) = 0;

private:
    std::shared_ptr<threadkit::ThreadTask> Run() override final {
        ProcessPacket(m_conn);
        return std::shared_ptr<threadkit::ThreadTask>();
    }

private:
    Connection* m_conn;
    Buffer m_buf;

private:
    Processor(const Processor&);
    Processor& operator=(const Processor&);
};

} // namespace netkit

#endif
