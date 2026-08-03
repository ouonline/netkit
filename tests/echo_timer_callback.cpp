#include "netkit/event_manager.h"
#include "netkit/timer.h"
using namespace netkit;

#include "logger/stdout_logger.h"

#include <new>
#include <string.h> // strerror()
#include <string>
using namespace std;

class EchoTask final : public Task {
public:
    void Run(SendContext* ctx) override {
        const auto& info = ctx->GetEndpointInfo();
        logger_info(logger(),
                    "[client] server [%s:%u] ==> client [%s:%u] data [%.*s]",
                    info.remote_addr.c_str(), info.remote_port,
                    info.local_addr.c_str(), info.local_port,
                    static_cast<int>(m_buffer.size()), m_buffer.data());
    }
};

class EchoTimer final : public Timer {
private:
    bool OnExpiration(int val, SendContext* ctx) override {
        if (val < 0) {
            logger_error(logger(), "timer failed: [%s].", strerror(-val));
            return false;
        }

        Buffer buf;
        const auto message = to_string(m_counter++);
        int err = buf.Append(message.data(), message.size());
        if (err) {
            logger_error(logger(), "prepare data failed: [%s].",
                         strerror(-err));
            return false;
        }

        const auto& info = ctx->GetEndpointInfo();
        logger_info(
            logger(), "[client] client [%s:%u] ==> server [%s:%u] data [%.*s]",
            info.local_addr.c_str(), info.local_port, info.remote_addr.c_str(),
            info.remote_port, static_cast<int>(buf.size()), buf.data());

        err = ctx->Emit(move(buf));
        if (err) {
            logger_error(logger(), "send data failed: [%s].", strerror(-err));
            return false;
        }

        return true;
    }

private:
    uint64_t m_counter = 0;
};

class EchoClient final : public TcpClient {
public:
    ~EchoClient() {
        logger_info(logger(), "[client] client destroyed.");
    }

    int OnConnected(SendContext* ctx) override {
        m_endpoint_info = ctx->GetEndpointInfo();
        logger_info(logger(), "[client] connect to server [%s:%u].",
                    m_endpoint_info.remote_addr.c_str(),
                    m_endpoint_info.remote_port);

        TimerPtr timer(new EchoTimer());
        if (!timer) {
            logger_error(logger(), "allocate timer failed: [%s].",
                         strerror(ENOMEM));
            return -ENOMEM;
        }

        const TimeVal interval = {1, 0};
        const int timer_fd = ctx->AddTimer(interval, move(timer));
        if (timer_fd < 0) {
            logger_error(logger(), "add timer failed: [%s].",
                         strerror(-timer_fd));
            return timer_fd;
        }

        return 0;
    }

    void OnDisconnected() override {
        logger_info(logger(), "[client] client [%s:%u] disconnected.",
                    m_endpoint_info.local_addr.c_str(),
                    m_endpoint_info.local_port);
    }

    ReqStat Check(const Buffer& req, uint32_t* size) override {
        *size = req.size();
        return ReqStat::VALID;
    }

    TaskPtr CreateTask() override {
        return TaskPtr(new EchoTask());
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
    auto err = mgr.Init(EventManager::Options());
    if (err) {
        logger_error(&logger.l, "init manager failed: [%s].", strerror(-err));
        return -1;
    }

    err = mgr.AddTcpClient(host, port, TcpClientPtr(new EchoClient()));
    if (err < 0) {
        logger_error(&logger.l, "add client failed: [%s].", strerror(-err));
        return -1;
    }

    mgr.Loop();

    stdout_logger_destroy(&logger);

    return 0;
}
