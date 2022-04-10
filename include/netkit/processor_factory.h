#ifndef __NETKIT_PROCESSOR_FACTORY_H__
#define __NETKIT_PROCESSOR_FACTORY_H__

#include "processor.h"
#include "connection.h"

namespace outils { namespace net {

class ProcessorFactory {
public:
    virtual ~ProcessorFactory() {}
    virtual void OnClientConnected(Connection*) = 0;
    virtual void OnClientDisconnected(Connection*) = 0;
    virtual Processor* CreateProcessor() = 0;
    virtual void DestroyProcessor(Processor*) = 0;
};

}} // namespace outils::net

#endif
