#include "netkit/connection_manager.h"
using namespace netkit;

#include "logger/stdout_logger.h"
#include <cstring> // strerror()
using namespace std;

class EchoServerHandler final : public RequestHandler {
public:
    EchoServerHandler(Logger* logger) : m_logger(logger) {}

    ~EchoServerHandler() {
        logger_info(m_logger, "[server] session destroyed.");
    }

    void OnConnected(Sender* sender) override {
        const ConnectionInfo& info = sender->GetConnectionInfo();
        logger_info(m_logger, "[server] client [%s:%u] connected.",
                    info.remote_addr.c_str(), info.remote_port);
    }

    void OnDisconnected(const ConnectionInfo& info) override {
        logger_info(m_logger, "[server] client [%s:%u] disconnected.",
                    info.remote_addr.c_str(), info.remote_port);
    }

    ReqStat Check(const Buffer& req, uint64_t* size) override {
        *size = req.GetSize();
        return ReqStat::VALID;
    }

    void Process(Buffer&& req, Sender* sender) override {
        const ConnectionInfo& info = sender->GetConnectionInfo();
        logger_info(m_logger, "[server] client[%s:%u] ==> server[%s:%u] data[%.*s]",
                    info.local_addr.c_str(), info.local_port,
                    info.remote_addr.c_str(), info.remote_port,
                    req.GetSize(), req.GetData());
        auto err = sender->SendAsync(std::move(req));
        if (err) {
            logger_error(m_logger, "send data failed: [%s].", strerror(-err));
        }
    }

private:
    Logger* m_logger;
};

class EchoServerFactory final : public RequestHandlerFactory {
public:
    EchoServerFactory(Logger* logger) : m_logger(logger) {}
    RequestHandler* Create() override {
        return new EchoServerHandler(m_logger);
    }
    void Destroy(RequestHandler* p) override {
        delete p;
    }

private:
    Logger* m_logger;
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

    ConnectionManager mgr(&logger.l);
    if (mgr.Init() != 0) {
        logger_error(&logger.l, "init manager failed.");
        return -1;
    }

    auto err = mgr.AddServer(host, port, make_shared<EchoServerFactory>(&logger.l));
    if (err) {
        logger_error(&logger.l, "add server failed: [%s].", strerror(-err));
        return -1;
    }

    mgr.Loop();

    stdout_logger_destroy(&logger);

    return 0;
}
