#ifndef __NETKIT_PROCESSOR_H__
#define __NETKIT_PROCESSOR_H__

#include "buffer.h"
#include "connection.h"
#include "threadkit/threadpool.h"

namespace netkit {

class Processor : public threadkit::ThreadTask {
public:
    Processor() : m_conn(nullptr) {}
    virtual ~Processor() {}

    // sets `total_packet_bytes` to total length of current request
    virtual bool CheckPacket(uint32_t* total_packet_bytes) = 0;

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
