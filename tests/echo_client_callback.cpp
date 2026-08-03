#include "netkit/event_manager.h"
using namespace netkit;

#include "logger/stdout_logger.h"

#include <string.h> // strerror()
#include <unistd.h>
using namespace std;

class EchoTask final : public Task {
public:
    EchoTask(Logger* l) : Task(l) {}
    void Run(SendContext* ctx) override {
        auto& info = ctx->GetEndpointInfo();
        logger_info(
            m_logger, "[client] server [%s:%u] ==> client [%s:%u] data [%.*s]",
            info.remote_addr.c_str(), info.remote_port, info.local_addr.c_str(),
            info.local_port, m_buffer.size(), m_buffer.data());

        int err = m_buffer.Reserve(10);
        if (err) {
            logger_error(m_logger, "Reserve buffer failed: [%s].",
                         strerror(-err));
            return;
        }

        m_buffer.Append("\0", 1);
        auto num = atol(m_buffer.data());
        auto len = snprintf(m_buffer.data(), 10, "%ld", num + 1);
        m_buffer.Resize(len);

        err = ctx->Emit(move(m_buffer));
        if (err) {
            logger_error(m_logger, "send data failed: [%s].", strerror(-err));
        }

        sleep(1);
    }
};

class EchoClient final : public TcpClient {
public:
    EchoClient(Logger* l) : TcpClient(l) {}
    ~EchoClient() {
        logger_info(m_logger, "[client] cient destroyed.");
    }

    int OnConnected(SendContext* ctx) override {
        m_endpoint_info = ctx->GetEndpointInfo();
        logger_info(m_logger, "[client] connect to server [%s:%u].",
                    m_endpoint_info.remote_addr.c_str(),
                    m_endpoint_info.remote_port);

        Buffer buf;
        int err = buf.Append("0", 1);
        if (err) {
            logger_error(m_logger, "prepare init data failed: [%s].",
                         strerror(-err));
            return err;
        }

        logger_info(
            m_logger, "[client] client [%s:%u] ==> server [%s:%u] data [%.*s]",
            m_endpoint_info.local_addr.c_str(), m_endpoint_info.local_port,
            m_endpoint_info.remote_addr.c_str(), m_endpoint_info.remote_port,
            buf.size(), buf.data());

        err = ctx->Emit(move(buf));
        if (err) {
            logger_error(m_logger, "send data failed: [%s].", strerror(-err));
            return err;
        }

        sleep(1);
        return 0;
    }

    void OnDisconnected() override {
        logger_info(m_logger, "[client] client [%s:%u] disconnected.",
                    m_endpoint_info.local_addr.c_str(),
                    m_endpoint_info.local_port);
    }

    ReqStat Check(const Buffer& req, uint32_t* size) override {
        *size = req.size();
        return ReqStat::VALID;
    }

    TaskPtr CreateTask() override {
        return TaskPtr(new EchoTask(m_logger));
    }

private:
    EndpointInfo m_endpoint_info;
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
    int err = mgr.Init(EventManager::Options());
    if (err < 0) {
        logger_error(&logger.l, "init manager failed: [%s].", strerror(-err));
        return -1;
    }

    err = mgr.AddTcpClient(host, port, TcpClientPtr(new EchoClient(&logger.l)));
    if (err < 0) {
        logger_error(&logger.l, "add client failed: [%s].", strerror(-err));
        return -1;
    }

    mgr.Loop();

    stdout_logger_destroy(&logger);

    return 0;
}
