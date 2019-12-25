#include "lib-api.h"
#include "lib-main.h"

namespace Xn {

///////////////////////////////////////////////////////////////////////////////

void callEv(void *sender, const LibStdCallback &callback) {
	if (nullptr != callback.func)
		callback.func(sender, callback.data);
}

///////////////////////////////////////////////////////////////////////////////
// API

bool apiSupportsVersion(unsigned int version) {
}

int apiSetVersion(unsigned int version) {
}

unsigned int features() {
}

///////////////////////////////////////////////////////////////////////////////
// Connect / disconnect

int connect() {
	return 0;
}

int disconnect() {
	return 0;
}

bool connected() {
	return false;
}

///////////////////////////////////////////////////////////////////////////////

int trackStatus() {
}

void setTrackStatus(unsigned int trkStatus, LibStdCallback ok, LibStdCallback err) {
	lib.xn.setTrkStatus(
		static_cast<TrkStatus>(trkStatus),
		std::make_unique<Cb>([ok](void *s, void *) { callEv(s, ok); }),
		std::make_unique<Cb>([err](void *s, void *) { callEv(s, err); })
	);
}

///////////////////////////////////////////////////////////////////////////////

void emergencyStop(LibStdCallback ok, LibStdCallback err) {
}

void locoEmergencyStop(uint16_t addr, LibStdCallback ok, LibStdCallback err) {
}

void locoSetSpeed(uint16_t addr, int speed, int dir, LibStdCallback ok, LibStdCallback err) {
}

void locoSetFunc(uint16_t addr, uint32_t funcMask, uint32_t funcState, LibStdCallback ok,
                 LibStdCallback err) {
}

void locoAcquire(uint16_t addr, TrkAcquiredCallback, LibStdCallback err) {
}

void locoRelease(uint16_t addr, LibStdCallback ok) {
}

void pomWriteCv(uint16_t addr, uint16_t cv, uint8_t value, LibStdCallback ok, LibStdCallback err) {
}

///////////////////////////////////////////////////////////////////////////////
// Event binders

void bindBeforeOpen(TrkStdNotifyEvent f, void *data) {
	lib.events.bind(lib.events.beforeOpen, f, data);
}

void bindAfterOpen(TrkStdNotifyEvent f, void *data) {
	lib.events.bind(lib.events.afterOpen, f, data);
}

void bindBeforeClose(TrkStdNotifyEvent f, void *data) {
	lib.events.bind(lib.events.beforeClose, f, data);
}

void bindAfterClose(TrkStdNotifyEvent f, void *data) {
	lib.events.bind(lib.events.afterClose, f, data);
}

void bindOnTrackStatusChange(TrkStatusChangedEv f, void *data) {
	lib.events.bind(lib.events.onTrkStatusChanged, f, data);
}

void bindOnLog(TrkLogEv f, void *data) {
	lib.events.bind(lib.events.onLog, f, data);
}

void bindOnLocoStolen(TrkLocoEv f, void *data) {
	lib.events.bind(lib.events.onLocoStolen, f, data);
}

///////////////////////////////////////////////////////////////////////////////

void showConfigDialog() {
	lib.form.show();
}

///////////////////////////////////////////////////////////////////////////////

} // namespace Xn
