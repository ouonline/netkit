#include "netkit/event_manager.h"
using namespace netkit;

#include "logger/stdout_logger.h"
#include <cstring> // strerror()
using namespace std;

class EchoClientHandler final : public Handler {
public:
    EchoClientHandler(Logger* logger) : m_logger(logger) {}

    ~EchoClientHandler() {
        logger_info(m_logger, "[client] cient destroyed.");
    }

    void OnConnected(Sender* sender) override {
        const ConnectionInfo& info = sender->GetConnectionInfo();
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

        sleep(1);
        err = sender->SendAsync(std::move(buf));
        if (err) {
            logger_error(m_logger, "send data failed: [%s].", strerror(-err));
        }
    }

    void OnDisconnected(const ConnectionInfo& info) override {
        logger_info(m_logger, "[client] client [%s:%u] disconnected.",
                    info.local_addr.c_str(), info.local_port);
    }

    ReqStat Check(const Buffer& req, uint64_t* size) override {
        *size = req.GetSize();
        return ReqStat::VALID;
    }

    void Process(Buffer&& buf, Sender* sender) override {
        const ConnectionInfo& info = sender->GetConnectionInfo();
        logger_info(m_logger, "[client] server [%s:%u] ==> client [%s:%u] data [%.*s]",
                    info.remote_addr.c_str(), info.remote_port,
                    info.local_addr.c_str(), info.local_port,
                    buf.GetSize(), buf.GetData());

        int err = buf.Reserve(10);
        if (err) {
            logger_error(m_logger, "Reserve buffer failed: [%s].", strerror(-err));
            return;
        }

        buf.Append("\0", 1);
        auto num = atol(buf.GetData());
        auto len = snprintf(buf.GetData(), 10, "%ld", num + 1);
        buf.Resize(len);

        sleep(1);
        err = sender->SendAsync(std::move(buf));
        if (err) {
            logger_error(m_logger, "send data failed: [%s].", strerror(-err));
        }
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

    auto err = mgr.AddClient(host, port, make_shared<EchoClientHandler>(&logger.l));
    if (err) {
        logger_error(&logger.l, "add server failed: [%s].", strerror(-err));
        return -1;
    }

    mgr.Loop();

    stdout_logger_destroy(&logger);

    return 0;
}
