#include "netkit/utils.h"
#include "netkit/nq_utils.h"
using namespace netkit;

#include "logger/stdout_logger.h"
#include <vector>
#include <thread>
#include <cstring>
using namespace std;

#define ECHO_BUFFER_SIZE 1024

struct EchoClient final {
    int fd;
    uint32_t test_data_idx = 0;
    char buf[ECHO_BUFFER_SIZE];
};

/* ------------------------------------------------------------------------- */

#include <iostream>

static void PrepareTestDataLen(vector<uint32_t>* lens) {
#define N 55555
    lens->resize(N);
    srand(time(nullptr));
    for (int i = 0; i < N; ++i) {
        lens->at(i) = rand() % (ECHO_BUFFER_SIZE - 1) + 1;
    }
#undef N
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        cerr << "usage: " << argv[0] << " host port nr_client" << endl;
        return -1;
    }
    const char* host = argv[1];
    const uint16_t port = atol(argv[2]);
    const uint32_t nr_client = atol(argv[3]);

    vector<uint32_t> lens;
    char test_data_buf[ECHO_BUFFER_SIZE];
    PrepareTestDataLen(&lens);

    StdoutLogger logger;
    stdout_logger_init(&logger);

    NotificationQueueImpl send_nq;
    auto rc = InitNq(&send_nq, &logger.l);
    if (rc != 0) {
        logger_error(&logger.l, "init send queue failed.");
        return -1;
    }

    NotificationQueueImpl recv_nq;
    rc = InitNq(&recv_nq, &logger.l);
    if (rc != 0) {
        logger_error(&logger.l, "init recv queue failed.");
        return -1;
    }

    vector<EchoClient> client_list(nr_client);
    for (uint32_t i = 0; i < nr_client; ++i) {
        int ret = utils::CreateTcpClientFd(host, port, &logger.l);
        if (ret < 0) {
            logger_error(&logger.l, "init client connection failed: [%s].",
                         strerror(-ret));
            return -1;
        }
        client_list[i].fd = ret;
    }
    logger_info(&logger.l, "create [%u] clients.", nr_client);

    std::thread recv_thread([l = &logger.l, &client_list, &recv_nq]() -> void {
        for (auto it = client_list.begin(); it != client_list.end(); ++it) {
            auto client = &(*it);
            recv_nq.ReadAsync(client->fd, client->buf, ECHO_BUFFER_SIZE, client);
        }

        while (true) {
            int64_t res;
            void* tag;
            auto err = recv_nq.Next(&res, &tag, nullptr);
            if (err) {
                logger_error(l, "recv failed: [%s].", strerror(-err));
                break;
            }

            auto client = static_cast<EchoClient*>(tag);
            recv_nq.ReadAsync(client->fd, client->buf, ECHO_BUFFER_SIZE, client);
        }
    });

    // initial sending
    for (uint32_t i = 0; i < nr_client; ++i) {
        auto client = &client_list[i];
        rc = send_nq.WriteAsync(client->fd, test_data_buf, lens[0], client);
        if (rc != 0) {
            logger_error(&logger.l, "write initial request failed.");
            return -1;
        }
    }

    while (true) {
        int64_t res = 0;
        void* tag = nullptr;
        rc = send_nq.Next(&res, &tag, nullptr);
        if (rc != 0) {
            logger_error(&logger.l, "get event failed.");
            break;
        }

        auto client = static_cast<EchoClient*>(tag);
        auto len = lens[client->test_data_idx];
        client->test_data_idx = (client->test_data_idx + 1) % lens.size();
        send_nq.WriteAsync(client->fd, test_data_buf, len, client);
    }

    stdout_logger_destroy(&logger);

    return 0;
}
