#include "server_manager.h"
#include "logger.h"
using namespace utils::net;
using namespace utils::net::tcp;

#include <iostream>
using namespace std;

class TestProcessor final : public Processor {

public:
    bool CheckPacket(uint32_t* size) override {
        *size = 4;
        return true;
    }

protected:
    bool ProcessPacket() override {
        auto buf = GetPacket();
        auto c = GetConnection();

        const ConnectionInfo &info = c->GetConnectionInfo();

        log_info("client[%s:%u]: recv data size = %u, data -> %u",
                 info.addr.c_str(), info.port, buf->Size(),
                 *(uint32_t*)buf->Data());
        c->Send(buf->Data(), buf->Size());
        return true;
    }
};

class TestProcessorFactory final : public ProcessorFactory {

public:
    shared_ptr<Processor> CreateProcessor() override {
        return make_shared<TestProcessor>();
    }
};

int main(void) {
    ServerManager mgr;

    if (mgr.Init() != SC_OK) {
        cerr << "init manager failed." << endl;
        return -1;
    }

    if (mgr.AddServer("127.0.0.1", 12345, make_shared<TestProcessorFactory>()) != SC_OK) {
        cerr << "add server failed." << endl;
        return -1;
    }

    mgr.Run();

    return 0;
}
