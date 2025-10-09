#include "netkit/event_manager.h"
using namespace netkit;

#include "logger/stdout_logger.h"
#include <cstring> // strerror()
using namespace std;

class EchoServerHandler final : public Handler {
public:
    EchoServerHandler(Logger* logger) : m_logger(logger), m_conn(nullptr) {}

    ~EchoServerHandler() {
        logger_info(m_logger, "[server] session [%p] destroyed.", this);
    }

    int OnConnected(Connection* conn) override {
        m_conn = conn;
        const ConnectionInfo& info = conn->info();
        logger_info(m_logger, "[server] client [%s:%u] connected.",
                    info.remote_addr.c_str(), info.remote_port);
        return 0;
    }

    void OnDisconnected() override {
        const ConnectionInfo& info = m_conn->info();
        logger_info(m_logger, "[server] client [%s:%u] disconnected.",
                    info.remote_addr.c_str(), info.remote_port);
    }

    ReqStat Check(const Buffer& req, uint64_t* size) override {
        *size = req.size();
        return ReqStat::VALID;
    }

    void Process(Buffer&& req) override {
        const ConnectionInfo& info = m_conn->info();
        logger_info(
            m_logger, "[server] client[%s:%u] ==> server[%s:%u] data[%.*s]",
            info.local_addr.c_str(), info.local_port, info.remote_addr.c_str(),
            info.remote_port, req.size(), req.data());
        int err = m_conn->SendAsync(std::move(req));
        if (err) {
            logger_error(m_logger, "send data failed: [%s].", strerror(-err));
        }
    }

private:
    Logger* m_logger;
    Connection* m_conn;
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
    auto err = mgr.Init(EventManager::Options());
    if (err < 0) {
        logger_error(&logger.l, "init manager failed: [%s].", strerror(-err));
        return -1;
    }

    err = mgr.AddServer(host, port, make_shared<EchoServerFactory>(&logger.l));
    if (err < 0) {
        logger_error(&logger.l, "add server failed: [%s].", strerror(-err));
        return -1;
    }

    mgr.Loop();

    stdout_logger_destroy(&logger);

    return 0;
}
