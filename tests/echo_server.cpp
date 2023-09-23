#ifdef NETKIT_ENABLE_IOURING
#include "netkit/iouring/connection_manager.h"
using namespace netkit::iouring;
#elif defined NETKIT_ENABLE_EPOLL
#include "netkit/epoll/connection_manager.h"
using namespace netkit::epoll;
#endif
using namespace netkit;

#include "logger/stdio_logger.h"
#include <cstring> // strerror()
using namespace std;

#define ECHO_BUFFER_SIZE 1024

struct State {
    enum Value {
        UNKNOWN,
        SERVER_CLIENT_CONNECTED,
        SERVER_GET_REQ,
        SERVER_SEND_RES,
    } value;
    State(Value v = UNKNOWN) : value(v) {}
    virtual ~State() {}
};

struct EchoClient final : public State {
    Connection conn;
    char buf[ECHO_BUFFER_SIZE];
};

static void Process(int64_t res, void* tag, ConnectionManager* mgr, Logger* logger) {
    auto state = static_cast<State*>(tag);
    switch (state->value) {
        case State::SERVER_CLIENT_CONNECTED: {
            auto client = new EchoClient();
            auto rc = mgr->InitializeConnection(res, &client->conn);
            if (rc != RC_OK) {
                logger_error(logger, "init client connection failed.");
                break;
            }

            const ConnectionInfo& info = client->conn.GetInfo();
            logger_info(logger, "[server] accepts client [%s:%u].", info.remote_addr.c_str(), info.remote_port);

            client->value = State::SERVER_GET_REQ;
            client->conn.ReadAsync(client->buf, ECHO_BUFFER_SIZE, client);
            break;
        }
        case State::SERVER_GET_REQ: {
            auto client = static_cast<EchoClient*>(state);
            const ConnectionInfo& info = client->conn.GetInfo();
            if (res < 0) {
                logger_error(logger, "read client request failed: [%s].", strerror(-res));
                delete client;
            } else if (res == 0) {
                logger_info(logger, "[server] client [%s:%u] disconnected.", info.remote_addr.c_str(),
                            info.remote_port);
                delete client;
            } else {
                logger_info(logger, "[server] client [%s:%u] ==> server [%s:%u] data [%.*s]", info.remote_addr.c_str(),
                            info.remote_port, info.local_addr.c_str(), info.local_port, res, client->buf);
                client->value = State::SERVER_SEND_RES;
                client->conn.WriteAsync(client->buf, res, client);
            }
            break;
        }
        case State::SERVER_SEND_RES: {
            auto client = static_cast<EchoClient*>(state);
            if (res < 0) {
                logger_error(logger, "send response to client failed: [%s].", strerror(-res));
                delete client;
            } else if (res == 0) {
                const ConnectionInfo& info = client->conn.GetInfo();
                logger_info(logger, "[server] client [%s:%u] disconnected.", info.remote_addr.c_str(),
                            info.remote_port);
                delete client;
            } else {
                client->value = State::SERVER_GET_REQ;
                client->conn.ReadAsync(client->buf, ECHO_BUFFER_SIZE, client);
            }
            break;
        }
        default:
            logger_error(logger, "unknown state [%u].", state->value);
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

    State connected_state(State::SERVER_CLIENT_CONNECTED);
    auto svr = mgr.CreateTcpServer(host, port, &connected_state);
    if (!svr.IsValid()) {
        logger_error(&logger.l, "CreateTcpServer failed.");
        return -1;
    }

    while (true) {
        int64_t res = 0;
        void* tag = nullptr;

        auto rc = mgr.Wait(&res, &tag);
        if (rc != RC_OK) {
            logger_error(&logger.l, "get event failed.");
            break;
        }

        Process(res, tag, &mgr, &logger.l);
    }

    stdio_logger_destroy(&logger);

    return 0;
}
