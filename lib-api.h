#ifndef LIBAPI_H
#define LIBAPI_H

#include "xn.h"
#include "lib-api-common-def.h"

namespace Xn {

extern "C" {
XN_SHARED_EXPORT int CALL_CONV connect();
XN_SHARED_EXPORT int CALL_CONV disconnect();
XN_SHARED_EXPORT bool CALL_CONV connected();
}

} // namespace Xn

#endif
