#include "lib-api.h"
#include "lib-main.h"
#include "lib-errors.h"

namespace Xn {

///////////////////////////////////////////////////////////////////////////////

void callEv(void *sender, const LibStdCallback &callback) {
	if (nullptr != callback.func)
		callback.func(sender, callback.data);
}

///////////////////////////////////////////////////////////////////////////////
// API

bool apiSupportsVersion(unsigned int version) {
	return std::find(API_SUPPORTED_VERSIONS.begin(), API_SUPPORTED_VERSIONS.end(), version) !=
	       API_SUPPORTED_VERSIONS.end();
}

int apiSetVersion(unsigned int version) {
	if (!apiSupportsVersion(version))
		return TRK_UNSUPPORTED_API_VERSION;

	lib.api_version = version;
	return 0;
}

unsigned int features() {
	return 0; // no features yet
}

///////////////////////////////////////////////////////////////////////////////
// Connect / disconnect

int connect() {
	if (lib.xn.connected())
		return TRK_ALREADY_OPENNED;

	lib.events.call(lib.events.beforeOpen);

	try {
		lib.xn.connect(lib.s["XN"]["port"].toString(), lib.s["XN"]["baudrate"].toInt(),
		               static_cast<QSerialPort::FlowControl>(lib.s["XN"]["flowcontrol"].toInt()),
		               lib.interface(lib.s["XN"]["interface"].toString()));
	} catch (const Xn::QStrException &e) {
		const QString errMsg = "XN connect error while opening serial port '" +
			lib.s["XN"]["port"].toString() + "': " + e;
		lib.log(errMsg, LogLevel::Error);
		lib.events.call(lib.events.afterClose);
		lib.guiOnClose();
		return TRK_CANNOT_OPEN_PORT;
	}

	return 0;
}

int disconnect() {
	lib.events.call(lib.events.beforeClose);

	if (!lib.xn.connected())
		return TRK_NOT_OPENED;

	lib.opening = false;
	try {
		lib.xn.disconnect();
	} catch (const Xn::QStrException &e) {
		lib.log("XN disconnect error while closing serial port:" + e, LogLevel::Error);
	}

	return 0;
}

bool connected() {
	return lib.xn.connected();
}

///////////////////////////////////////////////////////////////////////////////

int trackStatus() {
	return static_cast<int>(lib.xn.getTrkStatus());
}

void setTrackStatus(unsigned int trkStatus, LibStdCallback ok, LibStdCallback err) {
	try {
		lib.xn.setTrkStatus(
			static_cast<TrkStatus>(trkStatus),
			std::make_unique<Cb>([ok](void *s, void *) { callEv(s, ok); }),
			std::make_unique<Cb>([err](void *s, void *) { callEv(s, err); })
		);
	} catch (...) {
		callEv(&lib.xn, err);
	}
}

///////////////////////////////////////////////////////////////////////////////

void emergencyStop(LibStdCallback ok, LibStdCallback err) {
	try {
		lib.xn.emergencyStop(
			std::make_unique<Cb>([ok](void *s, void *) { callEv(s, ok); }),
			std::make_unique<Cb>([err](void *s, void *) { callEv(s, err); })
		);
	} catch (...) {
		callEv(&lib.xn, err);
	}
}

void locoEmergencyStop(uint16_t addr, LibStdCallback ok, LibStdCallback err) {
	try {
		lib.xn.emergencyStop(
			LocoAddr(addr),
			std::make_unique<Cb>([ok](void *s, void *) { callEv(s, ok); }),
			std::make_unique<Cb>([err](void *s, void *) { callEv(s, err); })
		);
	} catch (...) {
		callEv(&lib.xn, err);
	}
}

void locoSetSpeed(uint16_t addr, int speed, bool dir, LibStdCallback ok, LibStdCallback err) {
	try {
		lib.xn.setSpeed(
			LocoAddr(addr), speed, static_cast<Direction>(dir),
			std::make_unique<Cb>([ok](void *s, void *) { callEv(s, ok); }),
			std::make_unique<Cb>([err](void *s, void *) { callEv(s, err); })
		);
	} catch (...) {
		callEv(&lib.xn, err);
	}
}

void locoSetFunc(uint16_t addr, uint32_t funcMask, uint32_t funcState, LibStdCallback ok,
                 LibStdCallback err) {
}

void locoAcquiredGotFunc(LocoInfo locoInfo, TrkAcquiredCallback acquired, void *, FC fc, FD fd) {
	for (size_t i = 0; i < 8; i++)
		if (fc.all & (1 << i))
			locoInfo.functions |= (1 << (i+13));
	for (size_t i = 0; i < 8; i++)
		if (fd.all & (1 << i))
			locoInfo.functions |= (1 << (i+21));

	if (acquired != nullptr)
		acquired(&lib.xn, locoInfo);
}

void locoAcquired(LocoAddr addr, TrkAcquiredCallback acquired, LibStdCallback err,
                  void *, bool used, Direction direction, unsigned speed, FA fa, FB fb) {
	LocoInfo locoInfo;
	locoInfo.addr = addr.addr;
	locoInfo.direction = static_cast<bool>(direction);
	locoInfo.speed = speed;
	locoInfo.maxSpeed = 28;
	locoInfo.usedByAnother = used;

	locoInfo.functions = 0;
	locoInfo.functions |= fa.sep.f0;
	for (size_t i = 0; i < 4; i++)
		if (fa.all & (1 << i))
			locoInfo.functions |= (1 << (i+1));
	for (size_t i = 0; i < 8; i++)
		if (fb.all & (1 << i))
			locoInfo.functions |= (1 << (i+5));

	try {
		lib.xn.getLocoFunc1328(
			addr,
			[locoInfo, acquired](void *s, FC fc, FD fd) {
				locoAcquiredGotFunc(locoInfo, acquired, s, fc, fd);
			},
			std::make_unique<Cb>([err](void *s, void *) { callEv(s, err); })
		);
	} catch (...) {
		callEv(&lib.xn, err);
	}
}

void locoAcquire(uint16_t addr, TrkAcquiredCallback acquired, LibStdCallback err) {
	try {
		lib.xn.getLocoInfo(
			LocoAddr(addr),
			[addr, acquired, err](void *s, bool used, Direction direction, unsigned speed, FA fa,
			                      FB fb) {
				locoAcquired(LocoAddr(addr), acquired, err, s, used, direction, speed, fa, fb);
			},
			std::make_unique<Cb>([err](void *s, void *) { callEv(s, err); })
		);
	} catch (...) {
		callEv(&lib.xn, err);
	}
}

void locoRelease(uint16_t addr, LibStdCallback ok) {
	(void)addr;
	callEv(&lib.xn, ok);
}

void pomWriteCv(uint16_t addr, uint16_t cv, uint8_t value, LibStdCallback ok, LibStdCallback err) {
	try {
		lib.xn.pomWriteCv(
			LocoAddr(addr), cv, value,
			std::make_unique<Cb>([ok](void *s, void *) { callEv(s, ok); }),
			std::make_unique<Cb>([err](void *s, void *) { callEv(s, err); })
		);
	} catch (...) {
		callEv(&lib.xn, err);
	}
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
