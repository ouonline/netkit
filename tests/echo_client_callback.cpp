#include "netkit/event_manager.h"
using namespace netkit;

#include "logger/stdout_logger.h"

#include <string.h> // strerror()
#include <unistd.h>
using namespace std;

class EchoClientHandler final : public Handler {
public:
    EchoClientHandler(Logger* logger) : m_logger(logger), m_conn(nullptr) {}

    ~EchoClientHandler() {
        logger_info(m_logger, "[client] cient destroyed.");
    }

    int OnConnected(Connection* conn) override {
        m_conn = conn;
        const ConnectionInfo& info = conn->info();
        logger_info(m_logger, "[client] connect to server [%s:%u].",
                    info.remote_addr.c_str(), info.remote_port);

        Buffer buf;
        int err = buf.Append("0", 1);
        if (err) {
            logger_error(m_logger, "prepare init data failed: [%s].",
                         strerror(-err));
            return err;
        }

        logger_info(m_logger, "[client] client [%s:%u] ==> server [%s:%u] data [%.*s]",
                    info.local_addr.c_str(), info.local_port,
                    info.remote_addr.c_str(), info.remote_port,
                    buf.size(), buf.data());

        err = conn->SendAsync(std::move(buf));
        if (err) {
            logger_error(m_logger, "send data failed: [%s].", strerror(-err));
            return err;
        }

        sleep(1);
        return 0;
    }

    void OnDisconnected() override {
        const ConnectionInfo& info = m_conn->info();
        logger_info(m_logger, "[client] client [%s:%u] disconnected.",
                    info.local_addr.c_str(), info.local_port);
    }

    ReqStat Check(const Buffer& req, uint64_t* size) override {
        *size = req.size();
        return ReqStat::VALID;
    }

    void Process(Buffer&& req) override {
        const ConnectionInfo& info = m_conn->info();
        logger_info(m_logger, "[client] server [%s:%u] ==> client [%s:%u] data [%.*s]",
                    info.remote_addr.c_str(), info.remote_port,
                    info.local_addr.c_str(), info.local_port,
                    req.size(), req.data());

        int err = req.Reserve(10);
        if (err) {
            logger_error(m_logger, "Reserve buffer failed: [%s].", strerror(-err));
            return;
        }

        req.Append("\0", 1);
        auto num = atol(req.data());
        auto len = snprintf(req.data(), 10, "%ld", num + 1);
        req.Resize(len);

        err = m_conn->SendAsync(std::move(req));
        if (err) {
            logger_error(m_logger, "send data failed: [%s].", strerror(-err));
        }

        sleep(1);
    }

private:
    Logger* m_logger;
    Connection* m_conn;
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

    err = mgr.AddClient(host, port, make_shared<EchoClientHandler>(&logger.l));
    if (err < 0) {
        logger_error(&logger.l, "add client failed: [%s].", strerror(-err));
        return -1;
    }

    mgr.Loop();

    stdout_logger_destroy(&logger);

    return 0;
}
