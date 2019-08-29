#include "processor_manager.h"
using namespace utils::net;
using namespace utils::net::tcp;

#include "deps/logger/global_logger.h"
#include <unistd.h>
#include <iostream>
using namespace std;

class EchoProcessor final : public Processor {

public:
    bool CheckPacket(uint32_t* size) override {
        auto buf = GetPacket();
        *size = buf->Size();
        return true;
    }

protected:
    bool ProcessPacket() override {
        auto buf = GetPacket();

        auto c = GetConnection();
        const ConnectionInfo &info = c->GetConnectionInfo();

        log_info("local[%s:%u]\t<=\tremote[%s:%u]\tdata[%s]",
                 info.local_addr.c_str(), info.local_port,
                 info.remote_addr.c_str(), info.remote_port,
                 string(buf->Data(), buf->Size()).c_str());
        sleep(1);

        auto num = std::stol(string(buf->Data(), buf->Size()));
        const string content = std::to_string(num + 1);
        c->Send(content.data(), content.size());

        log_info("local[%s:%u]\t=>\tremote[%s:%u]\tdata[%s]",
                 info.local_addr.c_str(), info.local_port,
                 info.remote_addr.c_str(), info.remote_port,
                 content.c_str());
        return true;
    }
};

class EchoServerFactory final : public ProcessorFactory {

public:
    void OnClientConnected(Connection*) override {}
    void OnClientDisconnected(Connection* c) override {
        const ConnectionInfo& info = c->GetConnectionInfo();
        log_info("client[%s:%u] disconnected.",
                 info.remote_addr.c_str(), info.remote_port);
    }
    shared_ptr<Processor> CreateProcessor() override {
        return make_shared<EchoProcessor>();
    }
};

class EchoClientFactory final : public ProcessorFactory {

public:
    void OnClientConnected(Connection* c) override {
        c->Send("0", 1);
    }
    void OnClientDisconnected(Connection* c) override {
        const ConnectionInfo& info = c->GetConnectionInfo();
        log_info("client[%s:%u] disconnected.",
                 info.local_addr.c_str(), info.local_port);
    }
    shared_ptr<Processor> CreateProcessor() override {
        return make_shared<EchoProcessor>();
    }
};

int main(void) {
    ProcessorManager mgr;

    log_init(nullptr, nullptr, 0, 0);

    if (mgr.Init() != SC_OK) {
        cerr << "init manager failed." << endl;
        return -1;
    }

    if (mgr.AddServer("127.0.0.1", 12345, make_shared<EchoServerFactory>()) != SC_OK) {
        cerr << "add server failed." << endl;
        return -1;
    }

    if (mgr.AddClient("127.0.0.1", 12345, make_shared<EchoClientFactory>()) != SC_OK) {
        cerr << "add client failed." << endl;
        return -1;
    }

    mgr.Run();

    return 0;
}
