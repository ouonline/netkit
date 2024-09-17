#include "netkit/utils.h"
#include "netkit/nq_utils.h"
using namespace netkit;

#include "logger/stdout_logger.h"
#include <unistd.h> // sleep()/close()
using namespace std;

#define ECHO_BUFFER_SIZE 1024

enum State {
    UNKNOWN,
    CLIENT_SEND_REQ,
    CLIENT_GET_RES,
    CLIENT_DISCONNECTED,
    CLIENT_END_LOOP,
};

struct EchoClient final {
    int fd;
    State state;
    ConnectionInfo info;
    char buf[ECHO_BUFFER_SIZE];
};

static State Process(int64_t res, EchoClient* client, NotificationQueueImpl* nq,
                     Logger* logger) {
    int rc;

    switch (client->state) {
        case State::CLIENT_SEND_REQ: {
            if (res == 0) {
                const ConnectionInfo& info = client->info;
                logger_info(logger, "[client] server [%s:%u] down.",
                            info.remote_addr.c_str(), info.remote_port);
                client->state = State::CLIENT_DISCONNECTED;
                rc = nq->CloseAsync(client->fd, client);
                if (rc != 0) {
                    logger_error(logger, "CloseAsync() failed.");
                }
                break;
            }

            client->state = State::CLIENT_GET_RES;
            rc = nq->RecvAsync(client->fd, client->buf, ECHO_BUFFER_SIZE, client);
            if (rc != 0) {
                logger_error(logger, "RecvAsync() failed.");
            }
            break;
        }
        case State::CLIENT_GET_RES: {
            const ConnectionInfo& info = client->info;
            if (res == 0) {
                logger_info(logger, "[client] server [%s:%u] down.",
                            info.remote_addr.c_str(), info.remote_port);
                client->state = State::CLIENT_DISCONNECTED;
                rc = nq->CloseAsync(client->fd, client);
                if (rc != 0) {
                    logger_error(logger, "CloseAsync() failed.");
                }
                break;
            }

            logger_info(logger, "[client] server [%s:%u] ==> client [%s:%u] data [%.*s]",
                        info.remote_addr.c_str(), info.remote_port,
                        info.local_addr.c_str(), info.local_port,
                        res, client->buf);
            sleep(1);

            if (res < ECHO_BUFFER_SIZE) {
                client->buf[res] = '\0';
            } else {
                client->buf[res - 1] = '\0';
            }

            auto num = atol(client->buf);
            if (num == 5) {
                client->state = State::CLIENT_DISCONNECTED;
                rc = nq->CloseAsync(client->fd, client);
                if (rc != 0) {
                    logger_error(logger, "CloseAsync() failed.");
                }
                break;
            }

            auto len = sprintf(client->buf, "%lu", num + 1);
            client->state = State::CLIENT_SEND_REQ;
            rc = nq->SendAsync(client->fd, client->buf, len, client);
            if (rc != 0) {
                logger_error(logger, "SendAsync() failed.");
            }
            break;
        }
        case State::CLIENT_DISCONNECTED: {
            logger_info(logger, "[client] client [%s:%u] closed.",
                        client->info.local_addr.c_str(), client->info.local_port);
            client->state = State::CLIENT_END_LOOP;
            break;
        }
        default:
            logger_error(logger, "unknown state [%u].", client->state);
            break;
    }

    return client->state;
}

/* ------------------------------------------------------------------------- */

#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "usage: " << argv[0] << " host port" << endl;
        return -1;
    }
    const char* host = argv[1];
    const uint16_t port = atol(argv[2]);

    StdoutLogger logger;
    stdout_logger_init(&logger);

    NotificationQueueImpl nq;
    auto rc = InitNq(&nq, &logger.l);
    if (rc < 0) {
        logger_error(&logger.l, "init notification queue failed: [%s].",
                     strerror(-rc));
        return -1;
    }

    EchoClient client;
    client.fd = utils::CreateTcpClientFd(host, port, &logger.l);
    if (client.fd < 0) {
        logger_error(&logger.l, "init client connection failed.");
        return -1;
    }

    utils::GenConnectionInfo(client.fd, &client.info);
    const ConnectionInfo& info = client.info;
    logger_info(&logger.l, "[client] client [%s:%u] connect to server [%s:%u].",
                info.local_addr.c_str(), info.local_port,
                info.remote_addr.c_str(), info.remote_port);

    client.state = State::CLIENT_SEND_REQ;
    rc = nq.SendAsync(client.fd, "0", 1, &client);
    if (rc != 0) {
        logger_error(&logger.l, "send initial request failed.");
        return -1;
    }

    while (true) {
        int64_t res = 0;
        void* tag = nullptr;

        rc = nq.Next(&res, &tag, nullptr);
        if (rc != 0) {
            logger_error(&logger.l, "get event failed.");
            break;
        }

        auto client = static_cast<EchoClient*>(tag);
        auto st = Process(res, client, &nq, &logger.l);
        if (st == State::CLIENT_END_LOOP) {
            break;
        }
    }

    stdout_logger_destroy(&logger);

    return 0;
}
