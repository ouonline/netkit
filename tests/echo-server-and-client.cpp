#include "netkit/processor_manager.h"
using namespace netkit;
using namespace netkit::tcp;

#include "logger/stdio_logger.h"
#include <unistd.h>
using namespace std;

class EchoServerProcessor final : public Processor {
public:
    EchoServerProcessor(Logger* logger) : m_logger(logger) {}
    ~EchoServerProcessor() {
        logger_info(m_logger, "[server] server processor destroyed.");
    }

    PacketState CheckPacket(Buffer* buf, uint64_t* size) override {
        *size = buf->GetSize();
        return Processor::PACKET_SUCCESS;
    }

    void OnConnected(Connection* c) override {
        const ConnectionInfo& info = c->GetConnectionInfo();
        logger_info(m_logger, "[server] accepts client[%s:%u].", info.remote_addr.c_str(), info.remote_port);
    }

    void OnDisconnected(Connection* c) override {
        const ConnectionInfo& info = c->GetConnectionInfo();
        logger_info(m_logger, "[server] client[%s:%u] disconnected.", info.remote_addr.c_str(), info.remote_port);
    }

    void ProcessPacket(Buffer* buf, Connection* c) override {
        const ConnectionInfo& info = c->GetConnectionInfo();
        logger_info(m_logger, "[server] client[%s:%u] ==> server[%s:%u] data[%s]", info.local_addr.c_str(), info.local_port,
                    info.remote_addr.c_str(), info.remote_port, string(buf->GetData(), buf->GetSize()).c_str());
        c->Send(buf->GetData(), buf->GetSize());
    }

private:
    Logger* m_logger;
};

class EchoServerFactory final : public ProcessorFactory {
public:
    EchoServerFactory(Logger* logger) : m_logger(logger) {}
    Processor* CreateProcessor() override {
        return new EchoServerProcessor(m_logger);
    }
    void DestroyProcessor(Processor* p) override {
        delete p;
    }

private:
    Logger* m_logger;
};

class EchoClientProcessor final : public Processor {
public:
    EchoClientProcessor(Logger* logger) : m_logger(logger) {}
    ~EchoClientProcessor() {
        logger_info(m_logger, "[client] client instance destroyed.");
    }

    PacketState CheckPacket(Buffer* buf, uint64_t* size) override {
        *size = buf->GetSize();
        return Processor::PACKET_SUCCESS;
    }

    void OnConnected(Connection* c) override {
        const ConnectionInfo& info = c->GetConnectionInfo();
        logger_info(m_logger, "[client] client[%s:%u] connected.", info.local_addr.c_str(), info.local_port);
        c->Send("0", 1);
    }

    void OnDisconnected(Connection* c) override {
        const ConnectionInfo& info = c->GetConnectionInfo();
        logger_info(m_logger, "[client] client[%s:%u] disconnected.", info.local_addr.c_str(), info.local_port);
    }

    void ProcessPacket(Buffer* buf, Connection* c) override {
        const ConnectionInfo& info = c->GetConnectionInfo();

        logger_info(m_logger, "[client] server[%s:%u] ==> client[%s:%u] data[%s]", info.local_addr.c_str(), info.local_port,
                    info.remote_addr.c_str(), info.remote_port, string(buf->GetData(), buf->GetSize()).c_str());
        sleep(1);

        auto num = std::stol(string(buf->GetData(), buf->GetSize()));
        const string content = std::to_string(num + 1);
        c->Send(content.data(), content.size());
    }

private:
    Logger* m_logger;
};

int main(void) {
    StdioLogger logger;
    stdio_logger_init(&logger);

    ProcessorManager mgr(&logger.l);
    if (mgr.Init() != RC_SUCCESS) {
        logger_error(&logger.l, "init manager failed.");
        return -1;
    }

    if (mgr.AddServer("127.0.0.1", 54321, make_shared<EchoServerFactory>(&logger.l)) != RC_SUCCESS) {
        logger_error(&logger.l, "add server failed.");
        return -1;
    }

    if (mgr.AddClient("127.0.0.1", 54321, make_shared<EchoClientProcessor>(&logger.l)) != RC_SUCCESS) {
        logger_error(&logger.l, "add client failed.");
        return -1;
    }

    mgr.Run();

    stdio_logger_destroy(&logger);

    return 0;
}
