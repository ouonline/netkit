#ifndef __NETKIT_UTILS_H__
#define __NETKIT_UTILS_H__

#include "netkit/retcode.h"
#include "netkit/connection_info.h"
#include "logger/logger.h"
#include <stdint.h>

namespace netkit { namespace utils {

int CreateTcpServerFd(const char* host, uint16_t port, Logger*);
int CreateTcpClientFd(const char* host, uint16_t port, Logger*);

RetCode SetNonBlocking(int fd, Logger*);

void GenConnectionInfo(int fd, ConnectionInfo*);

}}

#endif
