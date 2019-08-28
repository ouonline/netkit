#ifndef __NET_PROCESSOR_H__
#define __NET_PROCESSOR_H__

#include "buffer.h"
#include "connection.h"
#include "deps/threadpool/cpp/thread_pool.hpp"

namespace utils { namespace net {

class Processor : public ThreadTask {

public:
    virtual ~Processor() {}

    // sets `expected_size` to total length of request packet or 0 to recv data
    virtual bool CheckPacket(uint32_t* expected_size) = 0;

    Buffer* GetPacket() { return &m_buf; }

    void SetConnection(Connection* c) { m_conn = c; }
    Connection* GetConnection() { return m_conn; }

protected:
    virtual bool ProcessPacket() = 0;

private:
    bool IsFinished() const final { return m_is_finished; }
    void Process() final {
        ProcessPacket();
        m_is_finished = true;
    }

private:
    bool m_is_finished = false;
    Connection* m_conn = nullptr;
    Buffer m_buf;
};

}}

#endif
