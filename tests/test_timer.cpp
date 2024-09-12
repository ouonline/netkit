#include "logger/stdout_logger.h"
#include "netkit/event_manager.h"
using namespace netkit;

#include <iostream>
#include <string.h>
using namespace std;

int main(void) {
    StdoutLogger logger;
    stdout_logger_init(&logger);

    EventManager mgr(&logger.l);
    if (mgr.Init() != 0) {
        logger_error(&logger.l, "init manager failed.");
        return -1;
    }

    const TimeVal delay = {
        .tv_sec = 3,
        .tv_usec = 0,
    };
    const TimeVal interval = {
        .tv_sec = 1,
        .tv_usec = 0,
    };
    auto err = mgr.AddTimer(delay, interval,
                            [](int err, uint64_t nr_expiration) -> void {
                                cout << "expired event: " << nr_expiration << endl;
                            });
    if (err) {
        logger_error(&logger.l, "add timer failed: [%s].", strerror(-err));
        return -1;
    }

    mgr.Loop();

    stdout_logger_destroy(&logger);

    return 0;
}
