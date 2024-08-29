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

#include "logger/stdout_logger.h"
#include <unistd.h> // close()
#include <cstring> // strerror()
using namespace std;

#define ECHO_BUFFER_SIZE 1024

struct State {
    enum Value {
        UNKNOWN,
        CLIENT_CONNECTED,
        READ_REQ,
        WRITE_RES,
        CLOSED,
        END_LOOP,
    } value;
    State(Value v = UNKNOWN) : value(v) {}
    virtual ~State() {}
};

struct EchoServer final : public State {
    int64_t fd;
};

struct EchoSession final : public State {
    EchoSession() : fd(-1) {}
    ~EchoSession() {
        if (fd > 0) {
            close(fd);
        }
    }

    int64_t fd;
    ConnectionInfo info;
    char buf[ECHO_BUFFER_SIZE];
};

static State::Value Process(EchoServer* svr, int64_t res, void* tag, NotificationQueueImpl* nq, Logger* logger) {
    int rc;

    auto state = static_cast<State*>(tag);
    auto ret_state = state->value;
    switch (ret_state) {
        case State::CLIENT_CONNECTED: {
            if (res <= 0) {
                logger_info(logger, "[server] closes server: [%s].", strerror(-res));
                break;
            }

            auto session = new EchoSession();
            session->fd = res;
            utils::GenConnectionInfo(res, &session->info);
            logger_info(logger, "[server] accepts client [%s:%u].", session->info.remote_addr.c_str(),
                        session->info.remote_port);

            session->value = State::READ_REQ;
            rc = nq->ReadAsync(res, session->buf, ECHO_BUFFER_SIZE, static_cast<State*>(session));
            if (rc != 0) {
                logger_error(logger, "ReadAsync() failed.");
            }
            break;
        }
        case State::READ_REQ: {
            auto session = static_cast<EchoSession*>(state);
            const ConnectionInfo& info = session->info;
            if (res < 0) {
                logger_error(logger, "read session request failed: [%s].", strerror(-res));
                delete session;
            } else if (res == 0) {
                logger_info(logger, "[server] client [%s:%u] disconnected.", info.remote_addr.c_str(),
                            info.remote_port);
                delete session;
                svr->value = State::CLOSED;
                rc = nq->CloseAsync(svr->fd, static_cast<State*>(svr));
                if (rc != 0) {
                    logger_error(logger, "CloseAsync() failed.");
                }
            } else {
                logger_info(logger, "[server] client [%s:%u] ==> server [%s:%u] data [%.*s]", info.remote_addr.c_str(),
                            info.remote_port, info.local_addr.c_str(), info.local_port, res, session->buf);
                session->value = State::WRITE_RES;
                rc = nq->WriteAsync(session->fd, session->buf, res, tag);
                if (rc != 0) {
                    logger_error(logger, "WriteAsync() failed.");
                }
            }
            break;
        }
        case State::WRITE_RES: {
            auto session = static_cast<EchoSession*>(state);
            if (res < 0) {
                logger_error(logger, "write response to session failed: [%s].", strerror(-res));
                delete session;
            } else if (res == 0) {
                const ConnectionInfo& info = session->info;
                logger_info(logger, "[server] client [%s:%u] disconnected.", info.remote_addr.c_str(),
                            info.remote_port);
                delete session;
            } else {
                session->value = State::READ_REQ;
                rc = nq->ReadAsync(session->fd, session->buf, ECHO_BUFFER_SIZE, tag);
                if (rc != 0) {
                    logger_error(logger, "ReadAsync() failed.");
                }
            }
            break;
        }
        case State::CLOSED: {
            ret_state = State::END_LOOP;
            logger_info(logger, "[server] server closed.");
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

    StdoutLogger logger;
    stdout_logger_init(&logger);

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

    svr.value = State::CLIENT_CONNECTED;
    rc = nq.MultiAcceptAsync(svr.fd, static_cast<State*>(&svr));
    if (rc != 0) {
        logger_error(&logger.l, "register server to notification queue failed.");
        return -1;
    }

    while (true) {
        int64_t res = 0;
        void* tag = nullptr;

        auto rc = nq.Next(&res, &tag, nullptr);
        if (rc != 0) {
            logger_error(&logger.l, "get event failed.");
            break;
        }

        auto st = Process(&svr, res, tag, &nq, &logger.l);
        if (st == State::END_LOOP) {
            break;
        }
    }

    stdout_logger_destroy(&logger);

    return 0;
}
