#include "internal_utils.h"
#include "internal_timer.h"
#include <unistd.h> // close()
#include <string.h> // strerror()
#include <sys/timerfd.h>
using namespace std;

namespace netkit { namespace utils {

int AddTimer(const TimeVal& delay, const TimeVal& interval, const function<int(int32_t)>& cb,
             NotificationQueueImpl* nq, InternalClient* client, Logger* logger) {
    if (delay.tv_sec == 0 && delay.tv_usec == 0) {
        logger_error(logger, "delay == 0 means disarming this timer and is not allowed currently.");
        return -EINVAL;
    }

    int err = utils::InitThreadLocalNq(logger);
    if (err) {
        logger_error(logger, "init thread local logger failed: [%s].",
                     strerror(-err));
        return err;
    }

    int fd = timerfd_create(CLOCK_BOOTTIME, TFD_NONBLOCK | TFD_CLOEXEC);
    if (fd < 0) {
        logger_error(logger, "create timerfd failed: [%s].", strerror(errno));
        return -errno;
    }

    auto timer = CreateInternalTimer(fd, client, cb);
    if (!timer) {
        logger_error(logger, "allocate InternalTimer failed: [%s].",
                     strerror(ENOMEM));
        close(fd);
        return -ENOMEM;
    }

    const struct itimerspec ts = {
        .it_interval = {
            .tv_sec = interval.tv_sec,
            .tv_nsec = interval.tv_usec * 1000,
        },
        .it_value = {
            .tv_sec = delay.tv_sec,
            .tv_nsec = delay.tv_usec * 1000,
        },
    };

    err = timerfd_settime(fd, 0, &ts, nullptr);
    if (err) {
        logger_error(logger, "timerfd_settime failed: [%s].", strerror(errno));
        DestroyInternalTimer(timer);
        return -errno;
    }

    timer->value = State::TIMER_NEXT;
    GetClient(client);
    err = utils::GetThreadLocalNq()->NotifyAsync(nq, 0, static_cast<State*>(timer));
    if (err) {
        logger_error(logger, "about to read from timerfd failed: [%s].",
                     strerror(-err));
        timer->callback(err);
        DestroyInternalTimer(timer);
        PutClient(client);
        return err;
    }

    return 0;
}

thread_local bool g_nq_inited = false;
thread_local NotificationQueueImpl g_nq;

int InitThreadLocalNq(Logger* l) {
    if (!g_nq_inited) {
        int err = InitNq(&g_nq, l);
        if (err) {
            return err;
        }
        g_nq_inited = true;
    }
    return 0;
}

NotificationQueueImpl* GetThreadLocalNq() {
    if (!g_nq_inited) {
        return nullptr;
    }
    return &g_nq;
}

}}
