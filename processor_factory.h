#ifndef __NET_PROCESSOR_FACTORY_H__
#define __NET_PROCESSOR_FACTORY_H__

#include "processor.h"
#include "connection.h"

namespace utils { namespace net {

class ProcessorFactory {

public:
    virtual ~ProcessorFactory() {}
    virtual void OnClientConnected(Connection*) = 0;
    virtual void OnClientDisconnected(Connection*) = 0;
    virtual std::shared_ptr<Processor> CreateProcessor() = 0;
};

}}

#endif
