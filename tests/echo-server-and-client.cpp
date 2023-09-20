#include "netkit/connection_manager.h"
using namespace netkit;

#include "logger/stdio_logger.h"
#include <unistd.h>
using namespace std;

#define ECHO_BUFFER_SIZE 1024

static void ServerDoEcho(const shared_ptr<char>& buf, uint64_t sz, const shared_ptr<Connection>& c, Logger* logger) {
    const Connection::Info& info = c->GetInfo();
    logger_info(logger, "[server] client [%s:%u] ==> server [%s:%u] data [%.*s]", info.local_addr.c_str(),
                info.local_port, info.remote_addr.c_str(), info.remote_port, sz, buf.get());
    c->WriteAsync(buf.get(), sz, [buf, c, logger](uint64_t) -> void {
        c->ReadAsync(buf.get(), ECHO_BUFFER_SIZE, [buf, c, logger](uint64_t bytes_read) -> void {
            if (bytes_read == 0) {
                const Connection::Info& info = c->GetInfo();
                logger_info(logger, "[server] client [%s:%u] disconnected.", info.remote_addr.c_str(), info.remote_port);
                return;
            }
            ServerDoEcho(buf, bytes_read, c, logger);
        });
    });
}

static void ServerAcceptClient(const shared_ptr<Connection>& c, Logger* logger) {
    const Connection::Info& info = c->GetInfo();
    logger_info(logger, "[server] accepts client [%s:%u].", info.remote_addr.c_str(), info.remote_port);

    shared_ptr<char> buf((char*)malloc(ECHO_BUFFER_SIZE), [](char* buf) -> void {
        free(buf);
    });
    c->ReadAsync(buf.get(), ECHO_BUFFER_SIZE, [buf, c, logger](uint64_t bytes_read) -> void {
        if (bytes_read == 0) {
            const Connection::Info& info = c->GetInfo();
            logger_info(logger, "[server] client [%s:%u] disconnected.", info.remote_addr.c_str(), info.remote_port);
            return;
        }
        ServerDoEcho(buf, bytes_read, c, logger);
    });
}

static void ClientGetResAndSend(const shared_ptr<char>& buf, const shared_ptr<Connection>& c, Logger* logger) {
    c->ReadAsync(buf.get(), ECHO_BUFFER_SIZE, [buf, c, logger](uint64_t bytes_read) -> void {
        const Connection::Info& info = c->GetInfo();
        logger_info(logger, "[client] server [%s:%u] ==> client [%s:%u] data [%.*s]", info.local_addr.c_str(),
                    info.local_port, info.remote_addr.c_str(), info.remote_port, bytes_read, buf.get());
        sleep(1);

        if (bytes_read < ECHO_BUFFER_SIZE) {
            buf.get()[bytes_read] = '\0';
        } else {
            buf.get()[bytes_read - 1] = '\0';
        }

        auto num = atol(buf.get());
        if (num == 5) {
            c->ShutDownAsync([c, logger]() -> void {
                logger_info(logger, "[client] shutdown.");
            });
            return;
        }

        auto len = sprintf(buf.get(), "%lu", num + 1);

        c->WriteAsync(buf.get(), len, [buf, c, logger](uint64_t bytes_written) -> void {
            if (bytes_written == 0) {
                const Connection::Info& info = c->GetInfo();
                logger_info(logger, "[client] server [%s:%u] down.", info.remote_addr.c_str(), info.remote_port);
                return;
            }
            ClientGetResAndSend(buf, c, logger);
        });
    });
}

static void ClientSendMessage(const shared_ptr<Connection>& c, Logger* logger) {
    const Connection::Info& info = c->GetInfo();
    logger_info(logger, "[client] client [%s:%u] connected.", info.local_addr.c_str(), info.local_port);

    shared_ptr<char> buf((char*)malloc(ECHO_BUFFER_SIZE), [](char* buf) -> void {
        free(buf);
    });
    c->WriteAsync("0", 1, [buf, c, logger](uint64_t) -> void {
        ClientGetResAndSend(buf, c, logger);
    });
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

    auto rc = mgr.CreateTcpServer(host, port, [&logger](const shared_ptr<Connection>& c) -> void {
        ServerAcceptClient(c, &logger.l);
    });
    if (rc != RC_OK) {
        logger_error(&logger.l, "add server failed.");
        return -1;
    }

    auto client = mgr.CreateTcpClient(host, port);
    if (!client) {
        logger_error(&logger.l, "add client failed.");
        return -1;
    }
    ClientSendMessage(client, &logger.l);

    mgr.Run();

    stdio_logger_destroy(&logger);

    return 0;
}
