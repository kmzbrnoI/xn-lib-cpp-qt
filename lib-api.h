#ifndef LIBAPI_H
#define LIBAPI_H

#include "xn.h"
#include "lib-api-common-def.h"
#include "lib-events.h"

namespace Xn {

constexpr std::array<unsigned int, 1> API_SUPPORTED_VERSIONS {
    0x0001, // v1.0
};

extern "C" {

using LibCallbackFunc = void CALL_CONV (*) (void *sender, void *data);

struct LibStdCallback {
	LibCallbackFunc func;
	void *const data;
};

struct LocoInfo {
	uint16_t addr;
	bool direction;
	uint8_t speed;
	uint8_t maxSpeed;
	uint32_t functions;
	bool usedByAnother;
};

using TrkAcquiredCallback = void CALL_CONV (*)(const void *sender, LocoInfo);

XN_SHARED_EXPORT bool CALL_CONV apiSupportsVersion(unsigned int version);
XN_SHARED_EXPORT int CALL_CONV apiSetVersion(unsigned int version);
XN_SHARED_EXPORT unsigned int CALL_CONV features();

XN_SHARED_EXPORT int CALL_CONV connect();
XN_SHARED_EXPORT int CALL_CONV disconnect();
XN_SHARED_EXPORT bool CALL_CONV connected();

XN_SHARED_EXPORT int CALL_CONV trackStatus();
XN_SHARED_EXPORT void CALL_CONV setTrackStatus(unsigned int trkStatus, LibStdCallback ok,
                                               LibStdCallback err);

XN_SHARED_EXPORT void CALL_CONV emergencyStop(LibStdCallback ok, LibStdCallback err);
XN_SHARED_EXPORT void CALL_CONV locoEmergencyStop(uint16_t addr, LibStdCallback ok,
                                                  LibStdCallback err);
XN_SHARED_EXPORT void CALL_CONV locoSetSpeed(uint16_t addr, int speed, bool dir, LibStdCallback ok,
                                             LibStdCallback err);
XN_SHARED_EXPORT void CALL_CONV locoSetFunc(uint16_t addr, uint32_t funcMask, uint32_t funcState,
                                            LibStdCallback ok, LibStdCallback err);
XN_SHARED_EXPORT void CALL_CONV locoAcquire(uint16_t addr, TrkAcquiredCallback,
                                            LibStdCallback err);
XN_SHARED_EXPORT void CALL_CONV locoRelease(uint16_t addr, LibStdCallback ok);

XN_SHARED_EXPORT void CALL_CONV pomWriteCv(uint16_t addr, uint16_t cv, uint8_t value,
                                           LibStdCallback ok, LibStdCallback err);

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
