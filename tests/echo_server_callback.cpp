#include "netkit/event_manager.h"
using namespace netkit;

#include "logger/stdout_logger.h"
#include <cstring> // strerror()
using namespace std;

class EchoTask final : public Task {
public:
    void Run(SendContext* ctx) override {
        auto& info = ctx->GetEndpointInfo();
        logger_info(
            logger(), "[server] client[%s:%u] ==> server[%s:%u] data[%.*s]",
            info.local_addr.c_str(), info.local_port, info.remote_addr.c_str(),
            info.remote_port, m_buffer.size(), m_buffer.data());
        int err = ctx->Emit(move(m_buffer));
        if (err) {
            logger_error(logger(), "send data failed: [%s].", strerror(-err));
        }
    }
};

class EchoClient final : public TcpClient {
public:
    ~EchoClient() {
        logger_info(logger(), "[server] client [%p] destroyed.", this);
    }

    int OnConnected(SendContext* ctx) override {
        m_endpoint_info = ctx->GetEndpointInfo();
        logger_info(logger(), "[server] client [%s:%u] connected.",
                    m_endpoint_info.remote_addr.c_str(),
                    m_endpoint_info.remote_port);
        return 0;
    }

    void OnDisconnected() override {
        logger_info(logger(), "[server] client [%s:%u] disconnected.",
                    m_endpoint_info.remote_addr.c_str(),
                    m_endpoint_info.remote_port);
    }

    ReqStat Check(const Buffer& req, uint32_t* size) override {
        *size = req.size();
        return ReqStat::VALID;
    }

    Task* CreateTask() override {
        return new EchoTask();
    }

private:
    EndpointInfo m_endpoint_info;
};

class EchoServer final : public TcpServer {
public:
    TcpClient* CreateClient() override {
        return new EchoClient();
    }
};

int main(int argc, char* argv[]) {
    StdoutLogger logger;
    stdout_logger_init(&logger);

    if (argc != 3) {
        logger_error(&logger.l, "usage: %s host port.", argv[0]);
        return -1;
    }

    const char* host = argv[1];
    const uint16_t port = atol(argv[2]);

    EventManager mgr(&logger.l);
    auto err = mgr.Init(EventManager::Options());
    if (err < 0) {
        logger_error(&logger.l, "init manager failed: [%s].", strerror(-err));
        return -1;
    }

    err = mgr.AddTcpServer(host, port, new EchoServer());
    if (err < 0) {
        logger_error(&logger.l, "add server failed: [%s].", strerror(-err));
        return -1;
    }

    mgr.Loop();

    stdout_logger_destroy(&logger);

    return 0;
}
