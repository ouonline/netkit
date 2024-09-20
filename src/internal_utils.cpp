#include "internal_utils.h"
#include "internal_timer.h"
#include <unistd.h> // close()
#include <string.h> // strerror()
#include <sys/timerfd.h>
using namespace std;

namespace netkit { namespace utils {

int AddTimer(const TimeVal& delay, const TimeVal& interval,
             const function<void(int32_t, Buffer*)>& cb,
             NotificationQueueImpl* nq, InternalClient* client,
             Logger* logger) {
    if (interval.tv_sec == 0 && interval.tv_usec == 0) {
        logger_error(logger, "interval == 0 is not supported.");
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
        timer->callback(err, nullptr);
        DestroyInternalTimer(timer);
        PutClient(client);
        return err;
    }

    return fd;
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
