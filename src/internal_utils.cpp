#include "internal_timer.h"
#include "netkit/tcp_client.h"
#include <unistd.h> // close()
#include <string.h>
#include <sys/timerfd.h>
using namespace std;

namespace netkit { namespace utils {

int DoAddTimer(const TimeVal& delay, const TimeVal& interval,
               const function<void(int32_t val)>& cb, NotificationQueue* nq,
               Logger* logger) {
    if (delay.tv_sec == 0 && delay.tv_usec == 0) {
        logger_error(logger,
                     "delay == 0 means disarming this timer and is not allowed "
                     "currently.");
        return -EINVAL;
    }

    int fd = timerfd_create(CLOCK_BOOTTIME, TFD_NONBLOCK | TFD_CLOEXEC);
    if (fd < 0) {
        logger_error(logger, "create timerfd failed: [%s].", strerror(errno));
        return -errno;
    }

    auto timer = new InternalTimer(fd, cb, logger);
    if (!timer) {
        logger_error(logger, "allocate InternalTimer failed: [%s].",
                     strerror(ENOMEM));
        close(fd);
        return -ENOMEM;
    }

    const struct itimerspec ts = {
        .it_interval =
            {
                .tv_sec = interval.tv_sec,
                .tv_nsec = interval.tv_usec * 1000,
            },
        .it_value =
            {
                .tv_sec = delay.tv_sec,
                .tv_nsec = delay.tv_usec * 1000,
            },
    };

    int err = timerfd_settime(fd, 0, &ts, nullptr);
    if (err) {
        logger_error(logger, "timerfd_settime failed: [%s].", strerror(errno));
        timer->DeleteSelf();
        return -errno;
    }

    err = timer->Start(nq);
    if (err) {
        logger_error(logger, "start timer failed: [%s].", strerror(-err));
        timer->DeleteSelf();
        return err;
    }

    return 0;
}

}}
