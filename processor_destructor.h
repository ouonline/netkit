#ifndef __TCP_PROCESSOR_DESTRUCTOR_H__
#define __TCP_PROCESSOR_DESTRUCTOR_H__

#include "threadkit/threadpool.h"
#include "processor_factory.h"

namespace outils { namespace net { namespace tcp {

class ProcessorDestructor final : public ThreadTaskDestructor {
public:
    void SetFactory(ProcessorFactory* f) {
        m_factory = f;
    }
    void Process(ThreadTask* t) override {
        m_factory->DestroyProcessor((Processor*)t);
    }
private:
    ProcessorFactory* m_factory;
};

}}}

#endif
