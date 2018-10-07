#ifndef _XN_TYPEDEFS_
#define _XN_TYPEDEFS_

#include <QDateTime>
#include <QByteArray>
#include <vector>
#include <memory>

#include "../q-str-exception.h"

namespace Xn {

using XnCommandCallbackFunc = void (*)(void* sender, void* data);

struct XnCommandCallback {
	XnCommandCallbackFunc const func;
	void* const data;

	XnCommandCallback(XnCommandCallbackFunc const func, void* const data = nullptr)
		: func(func), data(data) {}
};

using XnCb = XnCommandCallback;
using UPXnCb = std::unique_ptr<XnCommandCallback>;


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
struct EInvalidSpeed : public QStrException {
	EInvalidSpeed(const QString str) : QStrException(str) {}
};


struct LocoAddr {
	uint16_t addr;

	LocoAddr(uint16_t addr) : addr(addr) {
		if (addr > 9999)
			throw EInvalidAddr("Invalid loco address!");
	}

	LocoAddr(uint8_t lo, uint8_t hi) : LocoAddr(lo + ((hi-0xC0) << 8)) {}

	uint8_t lo() const { return addr & 0xFF; }
	uint8_t hi() const { return ((addr >> 8) & 0xFF) + (addr < 100 ? 0 : 0xC0); }

	operator uint16_t() const { return addr; }
	operator QString() const { return QString(addr); }
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

struct XnCmdEmergencyStop : public XnCmd {
	std::vector<uint8_t> getBytes() const override { return {0x80}; }
	QString msg() const override { return "All Loco Emergency Stop"; }
};

struct XnCmdEmergencyStopLoco : public XnCmd {
	const LocoAddr loco;

	XnCmdEmergencyStopLoco(const LocoAddr loco) : loco(loco) {}
	std::vector<uint8_t> getBytes() const override {
		return {0x92, loco.hi(), loco.lo()};
	}
	QString msg() const override {
		return "Single Loco Emergency Stop : " + QString(loco);
	}
};

using XnGotLIVersion = void (*)(void* sender, unsigned hw, unsigned sw);

struct XnCmdGetLIVersion : public XnCmd {
	XnGotLIVersion const callback;

	XnCmdGetLIVersion(XnGotLIVersion const callback) : callback(callback) {}
	std::vector<uint8_t> getBytes() const override { return {0xF0}; }
	QString msg() const override { return "LI Get Version"; }
};

using XnGotLIAddress = void (*)(void* sender, unsigned addr);

struct XnCmdGetLIAddress : public XnCmd {
	XnGotLIAddress const callback;

	XnCmdGetLIAddress(XnGotLIAddress const callback) : callback(callback) {}
	std::vector<uint8_t> getBytes() const override { return {0xF2, 0x01, 0x00}; }
	QString msg() const override { return "LI Get Address"; }
};

struct XnCmdSetLIAddress : public XnCmd {
	const unsigned addr;

	XnCmdSetLIAddress(const unsigned addr) : addr(addr) {}
	std::vector<uint8_t> getBytes() const override {
		return {0xF2, 0x01, static_cast<uint8_t>(addr)};
	}
	QString msg() const override { return "LI Set Address to " + QString(addr); }
};

using XnGotCSVersion = void (*)(void* sender, unsigned major, unsigned minor);

struct XnCmdGetCSVersion : public XnCmd {
	XnGotCSVersion const callback;

	XnCmdGetCSVersion(XnGotCSVersion const callback) : callback(callback) {}
	std::vector<uint8_t> getBytes() const override { return {0x21, 0x21}; }
	QString msg() const override { return "Get Command station version"; }
};

struct XnCmdGetCSStatus : public XnCmd {
	std::vector<uint8_t> getBytes() const override { return {0x21, 0x24}; }
	QString msg() const override { return "Get Command station status"; }
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
		return "POM Addr " + QString::number(loco.addr) + ", CV " + QString(cv) +
		       ", Value: " + QString::number(value);
		}
};

union XnFA {
	uint8_t all;
	struct {
		bool _ :3;
		bool f0 :1;
		bool f4 :1;
		bool f3 :1;
		bool f2 :1;
		bool f1 :1;
	} sep;

	XnFA(uint8_t fa) :all(fa) {}
	XnFA() :all(0) {}
};

union XnFB {
	uint8_t all;
	struct {
		bool f12 :1;
		bool f11 :1;
		bool f10 :1;
		bool f9 :1;
		bool f8 :1;
		bool f7 :1;
		bool f6 :1;
		bool f5 :1;
	} sep;

	XnFB(uint8_t fb) :all(fb) {}
	XnFB() :all(0) {}
};

using XnGotLocoInfo = void (*)(void* sender, bool used, bool direction,
                               unsigned speed, XnFA fa, XnFB fb);

struct XnCmdGetLocoInfo : public XnCmd {
	const LocoAddr loco;
	XnGotLocoInfo const callback;

	XnCmdGetLocoInfo(const LocoAddr loco, XnGotLocoInfo const callback)
		: loco(loco), callback(callback) {}
	std::vector<uint8_t> getBytes() const override { return {0xE3, 0x00, loco.hi(), loco.lo()}; }
	QString msg() const override { return "Get Loco Information " + QString::number(loco.addr); }
};

struct XnCmdSetSpeedDir : public XnCmd {
	const LocoAddr loco;
	const unsigned speed;
	const bool dir;

	XnCmdSetSpeedDir(const LocoAddr loco, const unsigned speed, const bool dir)
		: loco(loco), speed(speed), dir(dir) {
		if (speed > 28)
			throw EInvalidSpeed("Speed out of range!");
	}

	std::vector<uint8_t> getBytes() const override {
		unsigned sp;
		if (speed > 0)
			sp = speed + 3;
		else
			sp = 0;
		return {
			0xE4, 0x12, loco.hi(), loco.lo(),
			static_cast<uint8_t>((dir << 7) + ((sp >> 1) & 0x0F) + ((sp & 0x1) << 4))
		};
	}
	QString msg() const override {
		return "Loco " + QString::number(loco) + " Set Speed " + QString::number(speed) +
		       ", Dir " + QString::number(dir);
	}
};

struct XnHistoryItem {
	XnHistoryItem(std::unique_ptr<const XnCmd>& cmd, QDateTime timeout, size_t no_sent,
	              std::unique_ptr<XnCb>&& callback_ok,
	              std::unique_ptr<XnCb>&& callback_err)
		: cmd(std::move(cmd)), timeout(timeout), no_sent(no_sent),
		  callback_ok(std::move(callback_ok)), callback_err(std::move(callback_err))
		{}
	XnHistoryItem(XnHistoryItem&& hist)
		: cmd(std::move(hist.cmd)), timeout(hist.timeout), no_sent(hist.no_sent),
		  callback_ok(std::move(hist.callback_ok)), callback_err(std::move(hist.callback_err))
		{}

	std::unique_ptr<const XnCmd> cmd;
	QDateTime timeout;
	size_t no_sent;
	std::unique_ptr<XnCb> callback_ok;
	std::unique_ptr<XnCb> callback_err;
};


enum class XnLogLevel {
	None = 0,
	Error = 1,
	Warning = 2,
	Info = 3,
	Data = 4,
	Debug = 5,
};

}//end namespace

#endif
