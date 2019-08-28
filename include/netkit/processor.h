#ifndef __NET_PROCESSOR_H__
#define __NET_PROCESSOR_H__

#include "buffer.h"
#include "connection.h"
#include "threadkit/threadpool.h"

namespace outils { namespace net {

class Processor : public ThreadTask {
public:
    Processor() : m_conn(nullptr) {}
    virtual ~Processor() {}

    // sets `expected_size` to total length of request packet or 0 to recv data
    virtual bool CheckPacket(uint32_t* expected_size) = 0;

    Buffer* GetPacket() {
        return &m_buf;
    }
    void SetConnection(Connection* c) {
        m_conn = c;
    }

protected:
    virtual bool ProcessPacket(Connection*) = 0;

private:
    std::shared_ptr<ThreadTask> Run() override final {
        ProcessPacket(m_conn);
        return std::shared_ptr<ThreadTask>();
    }

private:
    Connection* m_conn;
    Buffer m_buf;

private:
    Processor(const Processor&);
    Processor& operator=(const Processor&);
};

}} // namespace outils::net

#endif
