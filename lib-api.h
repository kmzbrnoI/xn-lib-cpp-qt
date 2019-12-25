#ifndef LIBAPI_H
#define LIBAPI_H

#include "xn.h"
#include "lib-api-common-def.h"
#include "lib-events.h"

namespace Xn {

extern "C" {
XN_SHARED_EXPORT int CALL_CONV connect();
XN_SHARED_EXPORT int CALL_CONV disconnect();
XN_SHARED_EXPORT bool CALL_CONV connected();

XN_SHARED_EXPORT void CALL_CONV bindBeforeOpen(TrkStdNotifyEvent f, void *data);
XN_SHARED_EXPORT void CALL_CONV bindAfterOpen(TrkStdNotifyEvent f, void *data);
XN_SHARED_EXPORT void CALL_CONV bindBeforeClose(TrkStdNotifyEvent f, void *data);
XN_SHARED_EXPORT void CALL_CONV bindAfterClose(TrkStdNotifyEvent f, void *data);
XN_SHARED_EXPORT void CALL_CONV bindOnTrackStatusChange(TrkStatusChangedEv f, void *data);
XN_SHARED_EXPORT void CALL_CONV bindOnLog(TrkLogEv f, void *data);
XN_SHARED_EXPORT void CALL_CONV bindOnLocoStolen(TrkLocoEv f, void *data);
}

} // namespace Xn

#endif
