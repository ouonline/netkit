#ifndef __NET_PROCESSOR_H__
#define __NET_PROCESSOR_H__

#include "buffer.h"
#include "connection.h"
#include "threadkit/threadpool.h"

namespace utils { namespace net {

class Processor : public ThreadTask {

public:
    virtual ~Processor() {}

    // sets `expected_size` to total length of request packet or 0 to recv data
    virtual bool CheckPacket(uint32_t* expected_size) = 0;

    Buffer* GetPacket() { return &m_buf; }
    void SetConnection(Connection* c) { m_conn = c; }

protected:
    virtual bool ProcessPacket(Connection*) = 0;

private:
    ThreadTaskInfo Run() override final {
        ProcessPacket(m_conn);
        return ThreadTaskInfo();
    }

private:
    Connection* m_conn = nullptr;
    Buffer m_buf;
};

}}

#endif
