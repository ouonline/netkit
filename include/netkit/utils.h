#ifndef __NETKIT_UTILS_H__
#define __NETKIT_UTILS_H__

#include "endpoint_info.h"
#include "timeval.h"
#include "logger/logger.h"
#include <stdint.h>

namespace netkit { namespace utils {

/** @return fd or -errno  */
int CreateTcpServerFd(const char* host, uint16_t port, Logger*);

/** @return fd or -errno  */
int CreateTcpClientFd(const char* host, uint16_t port, Logger*);

/** @return fd or -errno  */
int CreateTimerFd(const TimeVal& delay, const TimeVal& interval, Logger*);

void GenEndpointInfo(int fd, EndpointInfo*);

}}

#endif
