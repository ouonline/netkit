#ifdef NETKIT_ENABLE_IOURING
#include "netkit/iouring/connection_manager.h"
using namespace netkit::iouring;
#elif defined NETKIT_ENABLE_EPOLL
#include "netkit/epoll/connection_manager.h"
using namespace netkit::epoll;
#endif
using namespace netkit;

#include "logger/stdio_logger.h"
#include <unistd.h> // sleep()
using namespace std;

#define ECHO_BUFFER_SIZE 1024

struct State {
    enum Value {
        UNKNOWN,
        CLIENT_SEND_REQ,
        CLIENT_GET_RES,
        CLIENT_SHUTDOWN,
    } value;
    State(Value v = UNKNOWN) : value(v) {}
    virtual ~State() {}
};

struct EchoClient final : public State {
    Connection conn;
    char buf[ECHO_BUFFER_SIZE];
};

static void Process(int64_t res, EchoClient* client, ConnectionManager* mgr, Logger* logger) {
    switch (client->value) {
        case State::CLIENT_SEND_REQ: {
            client->value = State::CLIENT_GET_RES;
            client->conn.ReadAsync(client->buf, ECHO_BUFFER_SIZE, client);
            break;
        }
        case State::CLIENT_GET_RES: {
            const ConnectionInfo& info = client->conn.GetInfo();
            if (res == 0) {
                logger_info(logger, "[client] server [%s:%u] down.", info.remote_addr.c_str(), info.remote_port);
                break;
            }

            logger_info(logger, "[client] server [%s:%u] ==> client [%s:%u] data [%.*s]", info.remote_addr.c_str(),
                        info.remote_port, info.local_addr.c_str(), info.local_port, res, client->buf);
            sleep(1);

            if (res < ECHO_BUFFER_SIZE) {
                client->buf[res] = '\0';
            } else {
                client->buf[res - 1] = '\0';
            }

            auto num = atol(client->buf);
            if (num == 5) {
                client->value = State::CLIENT_SHUTDOWN;
                client->conn.ShutDownAsync(client);
                break;
            }

            auto len = sprintf(client->buf, "%lu", num + 1);
            client->value = State::CLIENT_SEND_REQ;
            client->conn.WriteAsync(client->buf, len, client);
            break;
        }
        case State::CLIENT_SHUTDOWN: {
            const ConnectionInfo& info = client->conn.GetInfo();
            logger_info(logger, "[client] client [%s:%u] shutdown.", info.local_addr.c_str(), info.local_port);
            break;
        }
        default:
            logger_error(logger, "unknown state [%u].", client->value);
            break;
    }
}

#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "usage: " << argv[0] << " host port" << endl;
        return -1;
    }
    const char* host = argv[1];
    const uint16_t port = atol(argv[2]);

    StdioLogger logger;
    stdio_logger_init(&logger);

    ConnectionManager mgr(&logger.l);
    if (mgr.Init() != RC_OK) {
        logger_error(&logger.l, "init manager failed.");
        return -1;
    }

    EchoClient client;
    auto rc = mgr.CreateTcpClient(host, port, &client.conn);
    if (rc != RC_OK) {
        logger_error(&logger.l, "CreateTcpClient failed.");
        return -1;
    }

    const ConnectionInfo& info = client.conn.GetInfo();
    logger_info(&logger.l, "[client] client [%s:%u] connect to server [%s:%u].", info.local_addr.c_str(),
                info.local_port, info.remote_addr.c_str(), info.remote_port);

    client.value = State::CLIENT_SEND_REQ;
    client.conn.WriteAsync("0", 1, &client);

    while (true) {
        int64_t res = 0;
        void* tag = nullptr;

        rc = mgr.Wait(&res, &tag);
        if (rc != RC_OK) {
            logger_error(&logger.l, "get event failed.");
            break;
        }

        auto client = static_cast<EchoClient*>(static_cast<State*>(tag));
        Process(res, client, &mgr, &logger.l);
    }

    stdio_logger_destroy(&logger);

    return 0;
}
