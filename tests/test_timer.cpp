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
    auto err = mgr.Init();
    if (err < 0) {
        logger_error(&logger.l, "init manager failed: [%s].", strerror(-err));
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
    err = mgr.AddTimer(delay, interval, [](int32_t val) -> void {
        cout << "expired event: " << val << endl;
    });
    if (err < 0) {
        logger_error(&logger.l, "add timer failed: [%s].", strerror(-err));
        return -1;
    }

    mgr.Loop();

    stdout_logger_destroy(&logger);

    return 0;
}
