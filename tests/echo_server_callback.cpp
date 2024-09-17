#include "netkit/event_manager.h"
using namespace netkit;

#include "logger/stdout_logger.h"
#include <cstring> // strerror()
using namespace std;

class EchoServerHandler final : public Handler {
public:
    EchoServerHandler(Logger* logger) : m_logger(logger) {}

    ~EchoServerHandler() {
        logger_info(m_logger, "[server] session [%p] destroyed.", this);
    }

    void OnConnected(const ConnectionInfo& info, Buffer*) override {
        m_info = info;
        logger_info(m_logger, "[server] client [%s:%u] connected.",
                    info.remote_addr.c_str(), info.remote_port);
    }

    void OnDisconnected() override {
        logger_info(m_logger, "[server] client [%s:%u] disconnected.",
                    m_info.remote_addr.c_str(), m_info.remote_port);
    }

    ReqStat Check(const Buffer& req, uint64_t* size) override {
        *size = req.GetSize();
        return ReqStat::VALID;
    }

    void Process(Buffer&& req, Buffer* res) override {
        logger_info(m_logger, "[server] client[%s:%u] ==> server[%s:%u] data[%.*s]",
                    m_info.local_addr.c_str(), m_info.local_port,
                    m_info.remote_addr.c_str(), m_info.remote_port,
                    req.GetSize(), req.GetData());
        *res = std::move(req);
    }

private:
    Logger* m_logger;
    ConnectionInfo m_info;
};

class EchoServerFactory final : public HandlerFactory {
public:
    EchoServerFactory(Logger* logger) : m_logger(logger) {}
    Handler* Create() override {
        return new EchoServerHandler(m_logger);
    }
    void Destroy(Handler* p) override {
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

    EventManager mgr(&logger.l);
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
