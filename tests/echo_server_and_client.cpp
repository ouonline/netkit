#include "netkit/connection_manager.h"
using namespace netkit;

#include "logger/stdio_logger.h"
#include <unistd.h>
#include <memory>
using namespace std;

#define ECHO_BUFFER_SIZE 1024

struct State {
    enum Value {
        UNKNOWN,
        SERVER_CLIENT_CONNECTED,
        SERVER_GET_REQ,
        SERVER_SEND_RES,
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

static void Process(uint64_t res, void* tag, ConnectionManager* mgr, Logger* logger) {
    auto state = static_cast<State*>(tag);
    switch (state->value) {
        case State::SERVER_CLIENT_CONNECTED: {
            auto client = new EchoClient();
            mgr->InitializeConnection(res, &client->conn);

            const Connection::Info& info = client->conn.GetInfo();
            logger_info(logger, "[server] accepts client [%s:%u].", info.remote_addr.c_str(), info.remote_port);

            client->value = State::SERVER_GET_REQ;
            client->conn.RecvAsync(client->buf, ECHO_BUFFER_SIZE, client);
            break;
        }
        case State::SERVER_GET_REQ: {
            auto client = static_cast<EchoClient*>(state);
            const Connection::Info& info = client->conn.GetInfo();
            if (res == 0) {
                logger_info(logger, "[server] client [%s:%u] disconnected.", info.remote_addr.c_str(),
                            info.remote_port);
                delete client;
            } else {
                logger_info(logger, "[server] client [%s:%u] ==> server [%s:%u] data [%.*s]", info.local_addr.c_str(),
                            info.local_port, info.remote_addr.c_str(), info.remote_port, res, client->buf);
                client->value = State::SERVER_SEND_RES;
                client->conn.SendAsync(client->buf, res, client);
            }
            break;
        }
        case State::SERVER_SEND_RES: {
            auto client = static_cast<EchoClient*>(state);
            if (res == 0) {
                const Connection::Info& info = client->conn.GetInfo();
                logger_info(logger, "[server] client [%s:%u] disconnected.", info.remote_addr.c_str(),
                            info.remote_port);
                delete client;
            } else {
                client->value = State::SERVER_GET_REQ;
                client->conn.RecvAsync(client->buf, ECHO_BUFFER_SIZE, client);
            }
            break;
        }
        case State::CLIENT_SEND_REQ: {
            auto client = static_cast<EchoClient*>(state);
            client->value = State::CLIENT_GET_RES;
            client->conn.RecvAsync(client->buf, ECHO_BUFFER_SIZE, client);
            break;
        }
        case State::CLIENT_GET_RES: {
            auto client = static_cast<EchoClient*>(state);
            const Connection::Info& info = client->conn.GetInfo();
            if (res == 0) {
                logger_info(logger, "[client] server [%s:%u] down.", info.remote_addr.c_str(), info.remote_port);
                break;
            }

            logger_info(logger, "[client] server [%s:%u] ==> client [%s:%u] data [%.*s]", info.local_addr.c_str(),
                        info.local_port, info.remote_addr.c_str(), info.remote_port, res, client->buf);
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
            client->conn.SendAsync(client->buf, len, client);
            break;
        }
        case State::CLIENT_SHUTDOWN: {
            auto client = static_cast<EchoClient*>(state);
            const Connection::Info& info = client->conn.GetInfo();
            logger_info(logger, "[client] client [%s:%u] shutdown.", info.local_addr.c_str(), info.local_port);
            break;
        }
        default:
            logger_error(logger, "unknown state [%u].", state->value);
            break;
    }
}

int main(void) {
    const char* host = "127.0.0.1";
    const uint16_t port = 54321;

    StdioLogger logger;
    stdio_logger_init(&logger);

    ConnectionManager mgr(&logger.l);
    if (mgr.Init() != RC_OK) {
        logger_error(&logger.l, "init manager failed.");
        return -1;
    }

    State connected_state(State::SERVER_CLIENT_CONNECTED);
    auto rc = mgr.CreateTcpServer(host, port, &connected_state);
    if (rc != RC_OK) {
        logger_error(&logger.l, "CreateTcpServer failed.");
        return -1;
    }

    EchoClient client;
    rc = mgr.CreateTcpClient(host, port, &client.conn);
    if (rc != RC_OK) {
        logger_error(&logger.l, "CreateTcpClient failed.");
        return -1;
    }

    const Connection::Info& info = client.conn.GetInfo();
    logger_info(&logger.l, "[client] client [%s:%u] connected.", info.local_addr.c_str(), info.local_port);

    client.value = State::CLIENT_SEND_REQ;
    client.conn.SendAsync("0", 1, &client);

    mgr.Loop([&mgr, logger = &logger.l](uint64_t res, void* tag) -> void {
        Process(res, tag, &mgr, logger);
    });

    stdio_logger_destroy(&logger);

    return 0;
}
