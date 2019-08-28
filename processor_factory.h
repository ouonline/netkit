#ifndef __NET_PROCESSOR_FACTORY_H__
#define __NET_PROCESSOR_FACTORY_H__

#include "processor.h"

namespace utils { namespace net {

class ProcessorFactory {

public:
    virtual ~ProcessorFactory() {}
    virtual std::shared_ptr<Processor> CreateProcessor() = 0;
};

}}

#endif
