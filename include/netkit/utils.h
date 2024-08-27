#ifndef __NETKIT_UTILS_H__
#define __NETKIT_UTILS_H__

#include "connection_info.h"
#include "logger/logger.h"
#include <stdint.h>

namespace netkit { namespace utils {

/** @return fd or -errno  */
int CreateTcpServerFd(const char* host, uint16_t port, Logger*);

/** @return fd or -errno  */
int CreateTcpClientFd(const char* host, uint16_t port, Logger*);

void GenConnectionInfo(int fd, ConnectionInfo*);

}}

#endif
