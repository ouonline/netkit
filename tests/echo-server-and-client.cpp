#include "netkit/processor_manager.h"
using namespace outils::net;
using namespace outils::net::tcp;

#include "logger/stdio_logger.h"
#include <unistd.h>
using namespace std;

class EchoProcessor final : public Processor {

public:
    EchoProcessor(struct logger* logger) : m_logger(logger) {}

    bool CheckPacket(uint32_t* size) override {
        auto buf = GetPacket();
        *size = buf->Size();
        return true;
    }

protected:
    bool ProcessPacket(Connection* c) override {
        auto buf = GetPacket();
        const ConnectionInfo &info = c->GetConnectionInfo();

        logger_info(m_logger, "local[%s:%u]\t<=\tremote[%s:%u]\tdata[%s]",
                    info.local_addr.c_str(), info.local_port,
                    info.remote_addr.c_str(), info.remote_port,
                    string(buf->Data(), buf->Size()).c_str());
        sleep(1);

        auto num = std::stol(string(buf->Data(), buf->Size()));
        const string content = std::to_string(num + 1);
        c->Send(content.data(), content.size());

        logger_info(m_logger, "local[%s:%u]\t=>\tremote[%s:%u]\tdata[%s]",
                    info.local_addr.c_str(), info.local_port,
                    info.remote_addr.c_str(), info.remote_port,
                    content.c_str());
        return true;
    }

private:
    struct logger* m_logger;
};

class EchoServerFactory final : public ProcessorFactory {

public:
    EchoServerFactory(struct logger* logger) : m_logger(logger) {}
    void OnClientConnected(Connection*) override {}
    void OnClientDisconnected(Connection* c) override {
        const ConnectionInfo& info = c->GetConnectionInfo();
        logger_info(m_logger, "client[%s:%u] disconnected.",
                    info.remote_addr.c_str(), info.remote_port);
    }
    Processor* CreateProcessor() override {
        return new EchoProcessor(m_logger);
    }
    void DestroyProcessor(Processor* p) override {
        delete p;
    }

private:
    struct logger* m_logger;
};

class EchoClientFactory final : public ProcessorFactory {

public:
    EchoClientFactory(struct logger* logger) : m_logger(logger) {}
    void OnClientConnected(Connection* c) override {
        c->Send("0", 1);
    }
    void OnClientDisconnected(Connection* c) override {
        const ConnectionInfo& info = c->GetConnectionInfo();
        logger_info(m_logger, "client[%s:%u] disconnected.",
                    info.local_addr.c_str(), info.local_port);
    }
    Processor* CreateProcessor() override {
        return new EchoProcessor(m_logger);
    }
    void DestroyProcessor(Processor* p) override {
        delete p;
    }

private:
    struct logger* m_logger;
};

int main(void) {
    struct logger logger;
    stdio_logger_init(&logger);

    ProcessorManager mgr(&logger);
    if (mgr.Init() != SC_OK) {
        logger_error(&logger, "init manager failed.");
        return -1;
    }

    if (mgr.AddServer("127.0.0.1", 54321, make_shared<EchoServerFactory>(&logger)) != SC_OK) {
        logger_error(&logger, "add server failed.");
        return -1;
    }

    if (mgr.AddClient("127.0.0.1", 54321, make_shared<EchoClientFactory>(&logger)) != SC_OK) {
        logger_error(&logger, "add client failed.");
        return -1;
    }

    mgr.Run();

    return 0;
}
