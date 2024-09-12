#ifndef __NETKIT_HANDLER_FACTORY_H__
#define __NETKIT_HANDLER_FACTORY_H__

#include "handler.h"

namespace netkit {

class HandlerFactory {
public:
    virtual ~HandlerFactory() {}
    virtual Handler* Create() = 0;
    virtual void Destroy(Handler*) = 0;
};

}

#endif
