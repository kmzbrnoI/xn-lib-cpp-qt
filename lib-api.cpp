#include "lib-api.h"
#include "lib-main.h"

namespace Xn {

///////////////////////////////////////////////////////////////////////////////

void callEv(void *sender, const LibStdCallback &callback) {
	if (nullptr != callback.func)
		callback.func(sender, callback.data);
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

XN_SHARED_EXPORT void CALL_CONV setTrackStatus(unsigned int trkStatus, LibStdCallback ok,
                                               LibStdCallback err) {
	lib.xn.setTrkStatus(
		static_cast<TrkStatus>(trkStatus),
		std::make_unique<Cb>([ok](void *s, void *) { callEv(s, ok); }),
		std::make_unique<Cb>([err](void *s, void *) { callEv(s, err); })
	);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace Xn
