#ifdef NETKIT_ENABLE_IOURING
#include "netkit/iouring/notification_queue_impl.h"
using namespace netkit::iouring;
#elif defined NETKIT_ENABLE_EPOLL
#include "netkit/epoll/notification_queue_impl.h"
using namespace netkit::epoll;
#endif
#include "netkit/utils.h"
#include "netkit/connection_info.h"
using namespace netkit;

#include "logger/stdio_logger.h"
#include <sys/eventfd.h>
#include <unistd.h> // close()
#include <cstring> // strerror()
using namespace std;

#define ECHO_BUFFER_SIZE 1024

struct State {
    enum Value {
        UNKNOWN,
        SERVER_CLIENT_CONNECTED,
        SERVER_GET_REQ,
        SERVER_SEND_RES,
        SERVER_SHUTDOWN,
    } value;
    State(Value v = UNKNOWN) : value(v) {}
    virtual ~State() {}
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

static void Process(int64_t res, void* tag, NotificationQueueImpl* nq, int destroy_event_fd,
                    Logger* logger) {
    auto state = static_cast<State*>(tag);
    switch (state->value) {
        case State::SERVER_CLIENT_CONNECTED: {
            auto client = new EchoClient();
            client->fd = res;
            utils::GenConnectionInfo(res, &client->info);
            logger_info(logger, "[server] accepts client [%s:%u].", client->info.remote_addr.c_str(),
                        client->info.remote_port);

            client->value = State::SERVER_GET_REQ;
            nq->ReadAsync(res, client->buf, ECHO_BUFFER_SIZE, client);
            break;
        }
        case State::SERVER_GET_REQ: {
            auto client = static_cast<EchoClient*>(state);
            const ConnectionInfo& info = client->info;
            if (res < 0) {
                logger_error(logger, "read client request failed: [%s].", strerror(-res));
                delete client;
            } else if (res == 0) {
                logger_info(logger, "[server] client [%s:%u] disconnected.", info.remote_addr.c_str(),
                            info.remote_port);
                delete client;

                const uint64_t v = 1;
                write(destroy_event_fd, &v, sizeof(v));
            } else {
                logger_info(logger, "[server] client [%s:%u] ==> server [%s:%u] data [%.*s]", info.remote_addr.c_str(),
                            info.remote_port, info.local_addr.c_str(), info.local_port, res, client->buf);
                client->value = State::SERVER_SEND_RES;
                nq->WriteAsync(client->fd, client->buf, res, client);
            }
            break;
        }
        case State::SERVER_SEND_RES: {
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
                client->value = State::SERVER_GET_REQ;
                nq->ReadAsync(client->fd, client->buf, ECHO_BUFFER_SIZE, client);
            }
            break;
        }
        default:
            logger_error(logger, "unknown state [%u].", state->value);
            break;
    }
}

/* ------------------------------------------------------------------------- */

#include <iostream>
#include <memory>

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
    auto rc = nq.Init(false, &logger.l);
    if (rc != RC_OK) {
        logger_error(&logger.l, "init notification queue failed.");
        return -1;
    }

    uint64_t res_of_event_fd;
    int destroy_event_fd = eventfd(0, EFD_CLOEXEC);
    if (destroy_event_fd < 0) {
        logger_error(&logger.l, "create destroy event fd for notification queue failed.");
        return -1;
    }
    shared_ptr<void> __destroy_event_fd_destructor(nullptr, [destroy_event_fd](void*) -> void {
        close(destroy_event_fd);
    });

    State destroy_state(State::SERVER_SHUTDOWN);
    nq.ReadAsync(destroy_event_fd, &res_of_event_fd, sizeof(res_of_event_fd), &destroy_state);

    int svr_fd = utils::CreateTcpServerFd(host, port, &logger.l);
    if (svr_fd < 0) {
        logger_error(&logger.l, "init tcp server failed.");
        return -1;
    }
    shared_ptr<void> __svr_fd_destructor(nullptr, [svr_fd](void*) -> void {
        close(svr_fd);
    });

    State connected_state(State::SERVER_CLIENT_CONNECTED);
    rc = nq.MultiAcceptAsync(svr_fd, &connected_state);
    if (rc != RC_OK) {
        logger_error(&logger.l, "register server to notification queue failed.");
        return -1;
    }

    while (true) {
        int64_t res = 0;
        void* tag = nullptr;

        auto rc = nq.Wait(&res, &tag);
        if (rc != RC_OK) {
            logger_error(&logger.l, "get event failed.");
            break;
        }

        auto state = static_cast<State*>(tag);
        if (state->value == State::SERVER_SHUTDOWN) {
            break;
        }

        Process(res, tag, &nq, destroy_event_fd, &logger.l);
    }

    logger_info(&logger.l, "[server] server shutdown.");
    stdio_logger_destroy(&logger);

    return 0;
}
