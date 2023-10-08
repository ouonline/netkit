#include "netkit/utils.h"
using namespace netkit;

#ifdef NETKIT_ENABLE_IOURING

#include "netkit/iouring/notification_queue_impl.h"
using namespace netkit::iouring;

static int DoInitNq(NotificationQueueImpl* nq, Logger* l) {
    return nq->Init(NotificationQueueOptions(), l);
}

#elif defined NETKIT_ENABLE_EPOLL

#include "netkit/epoll/notification_queue_impl.h"
using namespace netkit::epoll;

static int DoInitNq(NotificationQueueImpl* nq, Logger* l) {
    return nq->Init(l);
}

#endif

#include "logger/stdio_logger.h"
#include <unistd.h> // close()
#include <cstring> // strerror()
using namespace std;

#define ECHO_BUFFER_SIZE 1024

struct State {
    enum Value {
        UNKNOWN,
        SERVER_CLIENT_CONNECTED,
        SERVER_CLIENT_GET_REQ,
        SERVER_CLIENT_SEND_RES,
        SERVER_CLOSED,
        SERVER_END_LOOP,
    } value;
    State(Value v = UNKNOWN) : value(v) {}
    virtual ~State() {}
};

struct EchoServer final : public State {
    int64_t fd;
};

struct EchoClient final : public State {
    EchoClient() : fd(-1) {}
    ~EchoClient() {
        if (fd > 0) {
            close(fd);
        }
    }

    int64_t fd;
    ConnectionInfo info;
    char buf[ECHO_BUFFER_SIZE];
};

static State::Value Process(EchoServer* svr, int64_t res, void* tag, NotificationQueueImpl* nq, Logger* logger) {
    auto state = static_cast<State*>(tag);
    auto ret_state = state->value;
    switch (ret_state) {
        case State::SERVER_CLIENT_CONNECTED: {
            if (res <= 0) {
                logger_info(logger, "[server] server shutdown: [%s].", strerror(-res));
                break;
            }

            auto client = new EchoClient();
            client->fd = res;
            utils::GenConnectionInfo(res, &client->info);
            logger_info(logger, "[server] accepts client [%s:%u].", client->info.remote_addr.c_str(),
                        client->info.remote_port);

            client->value = State::SERVER_CLIENT_GET_REQ;
            nq->ReadAsync(res, client->buf, ECHO_BUFFER_SIZE, static_cast<State*>(client));
            break;
        }
        case State::SERVER_CLIENT_GET_REQ: {
            auto client = static_cast<EchoClient*>(state);
            const ConnectionInfo& info = client->info;
            if (res < 0) {
                logger_error(logger, "read client request failed: [%s].", strerror(-res));
                delete client;
            } else if (res == 0) {
                logger_info(logger, "[server] client [%s:%u] disconnected.", info.remote_addr.c_str(),
                            info.remote_port);
                delete client;
                svr->value = State::SERVER_CLOSED;
                nq->CloseAsync(svr->fd, static_cast<State*>(svr));
            } else {
                logger_info(logger, "[server] client [%s:%u] ==> server [%s:%u] data [%.*s]", info.remote_addr.c_str(),
                            info.remote_port, info.local_addr.c_str(), info.local_port, res, client->buf);
                client->value = State::SERVER_CLIENT_SEND_RES;
                nq->WriteAsync(client->fd, client->buf, res, tag);
            }
            break;
        }
        case State::SERVER_CLIENT_SEND_RES: {
            auto client = static_cast<EchoClient*>(state);
            if (res < 0) {
                logger_error(logger, "send response to client failed: [%s].", strerror(-res));
                delete client;
            } else if (res == 0) {
                const ConnectionInfo& info = client->info;
                logger_info(logger, "[server] client [%s:%u] disconnected.", info.remote_addr.c_str(),
                            info.remote_port);
                delete client;
            } else {
                client->value = State::SERVER_CLIENT_GET_REQ;
                nq->ReadAsync(client->fd, client->buf, ECHO_BUFFER_SIZE, tag);
            }
            break;
        }
        case State::SERVER_CLOSED: {
            ret_state = State::SERVER_END_LOOP;
            logger_info(logger, "[server] server shutdown.");
            break;
        }
        default:
            logger_error(logger, "unknown state [%u].", state->value);
            break;
    }

    return ret_state;
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

    StdioLogger logger;
    stdio_logger_init(&logger);

    NotificationQueueImpl nq;
    auto rc = DoInitNq(&nq, &logger.l);
    if (rc != 0) {
        logger_error(&logger.l, "init notification queue failed.");
        return -1;
    }

    EchoServer svr;
    svr.fd = utils::CreateTcpServerFd(host, port, &logger.l);
    if (svr.fd < 0) {
        logger_error(&logger.l, "init tcp server failed.");
        return -1;
    }

    svr.value = State::SERVER_CLIENT_CONNECTED;
    rc = nq.MultiAcceptAsync(svr.fd, static_cast<State*>(&svr));
    if (rc != 0) {
        logger_error(&logger.l, "register server to notification queue failed.");
        return -1;
    }

    while (true) {
        int64_t res = 0;
        void* tag = nullptr;

        auto rc = nq.Wait(&res, &tag);
        if (rc != 0) {
            logger_error(&logger.l, "get event failed.");
            break;
        }

        auto st = Process(&svr, res, tag, &nq, &logger.l);
        if (st == State::SERVER_END_LOOP) {
            break;
        }
    }

    stdio_logger_destroy(&logger);

    return 0;
}
