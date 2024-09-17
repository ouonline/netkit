#include "netkit/event_manager.h"
using namespace netkit;

#include "logger/stdout_logger.h"

#include <string.h> // strerror()
#include <unistd.h>
using namespace std;

class EchoClientHandler final : public Handler {
public:
    EchoClientHandler(Logger* logger) : m_logger(logger) {}

    ~EchoClientHandler() {
        logger_info(m_logger, "[client] cient destroyed.");
    }

    void OnConnected(const ConnectionInfo& info, Buffer* res) override {
        m_info = info;
        logger_info(m_logger, "[client] connect to server [%s:%u].",
                    info.remote_addr.c_str(), info.remote_port);

        Buffer buf;
        int err = buf.Append("0", 1);
        if (err) {
            logger_error(m_logger, "prepare init data failed: [%s].",
                         strerror(-err));
            return;
        }

        logger_info(m_logger, "[client] client [%s:%u] ==> server [%s:%u] data [%.*s]",
                    info.local_addr.c_str(), info.local_port,
                    info.remote_addr.c_str(), info.remote_port,
                    buf.GetSize(), buf.GetData());
        *res = std::move(buf);
        sleep(1);
    }

    void OnDisconnected() override {
        logger_info(m_logger, "[client] client [%s:%u] disconnected.",
                    m_info.local_addr.c_str(), m_info.local_port);
    }

    ReqStat Check(const Buffer& req, uint64_t* size) override {
        *size = req.GetSize();
        return ReqStat::VALID;
    }

    void Process(Buffer&& req, Buffer* res) override {
        logger_info(m_logger, "[client] server [%s:%u] ==> client [%s:%u] data [%.*s]",
                    m_info.remote_addr.c_str(), m_info.remote_port,
                    m_info.local_addr.c_str(), m_info.local_port,
                    req.GetSize(), req.GetData());

        int err = req.Reserve(10);
        if (err) {
            logger_error(m_logger, "Reserve buffer failed: [%s].", strerror(-err));
            return;
        }

        req.Append("\0", 1);
        auto num = atol(req.GetData());
        auto len = snprintf(req.GetData(), 10, "%ld", num + 1);
        req.Resize(len);
        *res = std::move(req);
        sleep(1);
    }

private:
    Logger* m_logger;
    ConnectionInfo m_info;
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

    auto err = mgr.AddClient(host, port, make_shared<EchoClientHandler>(&logger.l));
    if (err) {
        logger_error(&logger.l, "add server failed: [%s].", strerror(-err));
        return -1;
    }

    mgr.Loop();

    stdout_logger_destroy(&logger);

    return 0;
}
