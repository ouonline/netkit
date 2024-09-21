#include "netkit/event_manager.h"
using namespace netkit;

#include "logger/stdout_logger.h"

#include <string.h> // strerror()
using namespace std;

class EchoClientHandler final : public Handler {
public:
    EchoClientHandler(Logger* logger) : m_logger(logger), m_conn(nullptr) {}

    ~EchoClientHandler() {
        logger_info(m_logger, "[client] cient destroyed.");
    }

    void OnConnected(Connection* conn, Buffer* out) override {
        m_conn = conn;
        const ConnectionInfo& info = conn->info();
        logger_info(m_logger, "[client] connect to server [%s:%u].",
                    info.remote_addr.c_str(), info.remote_port);

        auto s = std::to_string(m_counter);
        ++m_counter;
        int err = out->Append(s.data(), s.size());
        if (err) {
            logger_error(m_logger, "prepare init data failed: [%s].", strerror(-err));
            return;
        }
        logger_info(m_logger, "[client] client [%s:%u] ==> server [%s:%u] data [%.*s]",
                    info.local_addr.c_str(), info.local_port,
                    info.remote_addr.c_str(), info.remote_port,
                    out->size(), out->data());
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

    void Process(Buffer&& req, Buffer*) override {
        const ConnectionInfo& info = m_conn->info();
        logger_info(m_logger, "[client] server [%s:%u] ==> client [%s:%u] data [%.*s]",
                    info.remote_addr.c_str(), info.remote_port,
                    info.local_addr.c_str(), info.local_port,
                    req.size(), req.data());

        if (m_counter == 1) {
            const TimeVal delay = {
                .tv_sec = 2,
                .tv_usec = 0,
            };
            const TimeVal interval = {
                .tv_sec = 1,
                .tv_usec = 0,
            };
            m_conn->AddTimer(delay, interval, [&info, l = m_logger, counter = &m_counter]
                             (int32_t val, Buffer* out) -> void {
                if (val < 0) {
                    logger_error(l, "error: [%s].", strerror(-val));
                    return;
                }
                auto s = std::to_string(*counter);
                ++(*counter);
                int err = out->Append(s.data(), s.size());
                if (err) {
                    logger_error(l, "prepare init data failed: [%s].", strerror(-err));
                    return;
                }
                logger_info(l, "[client] client [%s:%u] ==> server [%s:%u] data [%.*s]",
                            info.local_addr.c_str(), info.local_port,
                            info.remote_addr.c_str(), info.remote_port,
                            out->size(), out->data());
            });
        }
    }

private:
    Logger* m_logger;
    Connection* m_conn;
    uint32_t m_counter = 0;
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
    auto err = mgr.Init();
    if (err < 0) {
        logger_error(&logger.l, "init manager failed: [%s].", strerror(-err));
        return -1;
    }

    err = mgr.AddClient(host, port, make_shared<EchoClientHandler>(&logger.l));
    if (err < 0) {
        logger_error(&logger.l, "add server failed: [%s].", strerror(-err));
        return -1;
    }

    mgr.Loop();

    stdout_logger_destroy(&logger);

    return 0;
}
