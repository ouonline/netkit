#ifndef __NETKIT_UTILS_H__
#define __NETKIT_UTILS_H__

#include "logger/logger.h"
#include <stdint.h>

namespace netkit { namespace utils {

int CreateTcpServerFd(const char* host, uint16_t port, Logger*);
int CreateTcpClientFd(const char* host, uint16_t port, Logger*);

}}

#endif
