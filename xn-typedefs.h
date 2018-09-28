#ifndef _XN_TYPEDEFS_
#define _XN_TYPEDEFS_

#include <QDateTime>
#include <QByteArray>
#include <vector>

#include "../q-str-exception.h"

using XnCommandCallbackFunc = void (*)(void* sender, void* data);

struct XnCommandCallback {
	XnCommandCallbackFunc const func;
	void* const data;

	XnCommandCallback(XnCommandCallbackFunc const func, void* const data = nullptr)
		: func(func), data(data) {}
};

using XnCb = XnCommandCallback;
using CPXnCb = const XnCommandCallback* const;


enum class XnTrkStatus {
	Unknown,
	Off,
	On,
	Programming,
};


struct EInvalidAddr : public QStrException {
	EInvalidAddr(const QString str) : QStrException(str) {}
};
struct EInvalidTrkStatus : public QStrException {
	EInvalidTrkStatus(const QString str) : QStrException(str) {}
};
struct EInvalidCv : public QStrException {
	EInvalidCv(const QString str) : QStrException(str) {}
};


struct LocoAddr {
	uint16_t addr;

	LocoAddr(uint16_t addr) : addr(addr) {
		if (addr > 9999)
			throw EInvalidAddr("Invalid loco address!");
	}

	LocoAddr(uint8_t lo, uint8_t hi) : LocoAddr(lo + ((hi-0xC0) << 8)) {}

	uint8_t lo() const { return addr & 0xFF; }
	uint8_t hi() const { return ((addr >> 8) & 0xFF) + 0xC0; }
};


struct XnCmd {
	virtual std::vector<uint8_t> getBytes() const = 0;
	virtual QString msg() const = 0;
	virtual ~XnCmd() {};
};

struct XnCmdOff : public XnCmd {
	std::vector<uint8_t> getBytes() const override { return {0x21, 0x80}; }
	QString msg() const override { return "Track Off"; }
};

struct XnCmdOn : public XnCmd {
	std::vector<uint8_t> getBytes() const override { return {0x21, 0x81}; }
	QString msg() const override { return "Track On"; }
};

struct XnCmdPomWriteCv : public XnCmd {
	const LocoAddr loco;
	const uint16_t cv;
	const uint8_t value;

	XnCmdPomWriteCv(const LocoAddr loco, const uint16_t cv, const uint8_t value)
		: loco(loco), cv(cv), value(value) {
		if (cv > 1023)
			throw EInvalidCv("CV value is too high!");
	}
	std::vector<uint8_t> getBytes() const override {
		return {
			0xE6, 0x30, loco.hi(), loco.lo(),
			static_cast<uint8_t>(0xEC + ((cv >> 8) & 0x03)),
			static_cast<uint8_t>(cv & 0xFF), value
		};
	}
	QString msg() const override {
		return "POM Addr " + QString(loco.addr) + ", CV " + QString(cv) +
		       ", Value: " + QString(value);
		}
};

struct XnHistoryItem {
	XnHistoryItem(const XnCmd& cmd, QDateTime timeout, size_t no_sent,
	              const XnCommandCallback* const callback_err, const XnCommandCallback* const callback_ok)
		: cmd(cmd), timeout(timeout), no_sent(no_sent), callback_err(callback_err),
		  callback_ok(callback_ok) {}

	const XnCmd& cmd;
	QDateTime timeout;
	size_t no_sent;
	const XnCommandCallback* const callback_err;
	const XnCommandCallback* const callback_ok;
};


enum class XnLogLevel {
	None = 0,
	Error = 1,
	Warning = 2,
	Info = 3,
	Data = 4,
	Debug = 5,
};

#endif
