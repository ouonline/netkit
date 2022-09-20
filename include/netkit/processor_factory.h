#ifndef __NETKIT_PROCESSOR_FACTORY_H__
#define __NETKIT_PROCESSOR_FACTORY_H__

#include "processor.h"
#include "connection.h"

namespace netkit {

class ProcessorFactory {
public:
    virtual ~ProcessorFactory() {}
    virtual Processor* CreateProcessor() = 0;
    virtual void DestroyProcessor(Processor*) = 0;
};

} // namespace netkit

#endif
