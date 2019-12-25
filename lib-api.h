#ifndef LIBAPI_H
#define LIBAPI_H

#include "xn.h"
#include "lib-api-common-def.h"
#include "lib-events.h"

namespace Xn {

extern "C" {

using LibCallbackFunc = void(*)(void *sender, void *data);

struct LibStdCallback {
	LibCallbackFunc func;
	void *const data;
};

XN_SHARED_EXPORT bool CALL_CONV apiSupportsVersion(unsigned int version);
XN_SHARED_EXPORT int CALL_CONV apiSetVersion(unsigned int version);
XN_SHARED_EXPORT unsigned int CALL_CONV features();

XN_SHARED_EXPORT int CALL_CONV connect();
XN_SHARED_EXPORT int CALL_CONV disconnect();
XN_SHARED_EXPORT bool CALL_CONV connected();

XN_SHARED_EXPORT int CALL_CONV trackStatus();
XN_SHARED_EXPORT void CALL_CONV setTrackStatus(unsigned int trkStatus, LibStdCallback ok,
                                               LibStdCallback err);

XN_SHARED_EXPORT void CALL_CONV bindBeforeOpen(TrkStdNotifyEvent f, void *data);
XN_SHARED_EXPORT void CALL_CONV bindAfterOpen(TrkStdNotifyEvent f, void *data);
XN_SHARED_EXPORT void CALL_CONV bindBeforeClose(TrkStdNotifyEvent f, void *data);
XN_SHARED_EXPORT void CALL_CONV bindAfterClose(TrkStdNotifyEvent f, void *data);
XN_SHARED_EXPORT void CALL_CONV bindOnTrackStatusChange(TrkStatusChangedEv f, void *data);
XN_SHARED_EXPORT void CALL_CONV bindOnLog(TrkLogEv f, void *data);
XN_SHARED_EXPORT void CALL_CONV bindOnLocoStolen(TrkLocoEv f, void *data);

XN_SHARED_EXPORT void CALL_CONV showConfigDialog();

}

} // namespace Xn

#endif
