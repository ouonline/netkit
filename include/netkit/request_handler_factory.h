#ifndef __NETKIT_REQUEST_HANDLER_FACTORY_H__
#define __NETKIT_REQUEST_HANDLER_FACTORY_H__

#include "request_handler.h"

namespace netkit {

class RequestHandlerFactory {
public:
    virtual ~RequestHandlerFactory() {}
    virtual RequestHandler* Create() = 0;
    virtual void Destroy(RequestHandler*) = 0;
};

}

#endif
