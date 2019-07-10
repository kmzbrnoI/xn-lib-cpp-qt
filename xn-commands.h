#ifndef XN_COMMANDS
#define XN_COMMANDS

/*
This file defines XpressNET commands.
Each command type has its own class inherited from XnCmd.
See xn.h or README for more documentation.
*/

#include <functional>
#include <vector>

#include "q-str-exception.h"
#include "xn-loco-addr.h"

namespace Xn {

struct EInvalidCv : public QStrException {
	EInvalidCv(const QString str) : QStrException(str) {}
};
struct EInvalidSpeed : public QStrException {
	EInvalidSpeed(const QString str) : QStrException(str) {}
};

struct XnCmd {
	virtual std::vector<uint8_t> getBytes() const = 0;
	virtual QString msg() const = 0;
	virtual ~XnCmd() = default;
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
	std::vector<uint8_t> getBytes() const override { return {0x92, loco.hi(), loco.lo()}; }
	QString msg() const override { return "Single Loco Emergency Stop : " + QString::number(loco); }
};

using XnGotLIVersion = std::function<void(void *sender, unsigned hw, unsigned sw)>;

struct XnCmdGetLIVersion : public XnCmd {
	XnGotLIVersion const callback;

	XnCmdGetLIVersion(XnGotLIVersion const callback) : callback(callback) {}
	std::vector<uint8_t> getBytes() const override { return {0xF0}; }
	QString msg() const override { return "LI Get Version"; }
};

using XnGotLIAddress = std::function<void(void *sender, unsigned addr)>;

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

using XnGotCSVersion = std::function<void(void *sender, unsigned major, unsigned minor)>;

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
			static_cast<uint8_t>(0xEC + (((cv-1) >> 8) & 0x03)),
			static_cast<uint8_t>((cv-1) & 0xFF), value
		};
	}
	QString msg() const override {
		return "POM Addr " + QString::number(loco.addr) + ", CV " + QString::number(cv) +
		       ", Value: " + QString::number(value);
	}
};

struct XnCmdPomWriteBit : public XnCmd {
	const LocoAddr loco;
	const uint16_t cv;
	const uint8_t biti;
	const bool value;

	XnCmdPomWriteBit(const LocoAddr loco, const uint16_t cv, const unsigned biti, const bool value)
	    : loco(loco), cv(cv), biti(biti), value(value) {
		if (cv > 1023)
			throw EInvalidCv("CV value is too high!");
	}
	std::vector<uint8_t> getBytes() const override {
		return {0xE6, 0x30, loco.hi(), loco.lo(),
		        static_cast<uint8_t>(0xE8 + (((cv-1) >> 8) & 0x03)),
		        static_cast<uint8_t>((cv-1) & 0xFF),
		        static_cast<uint8_t>(0xF0 + (value << 3) + biti)};
	}
	QString msg() const override {
		return "POM Addr " + QString::number(loco.addr) + ", CV " + QString::number(cv) +
		       ", Bit: " + QString::number(biti) + ", Value: " + QString::number(value);
	}
};

union XnFA {
	uint8_t all;
	struct {
		bool f1 : 1;
		bool f2 : 1;
		bool f3 : 1;
		bool f4 : 1;
		bool f0 : 1;
		bool _ : 3;
	} sep;

	XnFA(uint8_t fa) : all(fa) {}
	XnFA() : all(0) {}
};

union XnFB {
	uint8_t all;
	struct {
		bool f5 : 1;
		bool f6 : 1;
		bool f7 : 1;
		bool f8 : 1;
		bool f9 : 1;
		bool f10 : 1;
		bool f11 : 1;
		bool f12 : 1;
	} sep;

	XnFB(uint8_t fb) : all(fb) {}
	XnFB() : all(0) {}
};

enum class XnDirection {
	Backward = false,
	Forward = true,
};

using XnGotLocoInfo = std::function<void(void *sender, bool used, XnDirection direction,
                                         unsigned speed, XnFA fa, XnFB fb)>;

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
	const XnDirection dir;

	XnCmdSetSpeedDir(const LocoAddr loco, const unsigned speed, const XnDirection dir)
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
		return {0xE4, 0x12, loco.hi(), loco.lo(),
		        static_cast<uint8_t>((static_cast<bool>(dir) << 7) + ((sp >> 1) & 0x0F) +
		                             ((sp & 0x1) << 4))};
	}
	QString msg() const override {
		return "Loco " + QString::number(loco) + " Set Speed " + QString::number(speed) +
		       ", Dir " + QString::number(static_cast<int>(dir));
	}
};

struct XnCmdSetFuncA : public XnCmd {
	const LocoAddr loco;
	const XnFA fa;

	XnCmdSetFuncA(const LocoAddr loco, const XnFA fa) : loco(loco), fa(fa) {}
	std::vector<uint8_t> getBytes() const override {
		return {0xE4, 0x20, loco.hi(), loco.lo(), fa.all};
	}
	QString msg() const override {
		return "Set loco " + QString::number(loco.addr) + " func A: " + QString::number(fa.all, 2);
	}
};

enum class XnFSet {
	F5toF8,
	F9toF12,
};

struct XnCmdSetFuncB : public XnCmd {
	const LocoAddr loco;
	const XnFB fb;
	const XnFSet range;

	XnCmdSetFuncB(const LocoAddr loco, const XnFB fb, const XnFSet range)
	    : loco(loco), fb(fb), range(range) {}
	std::vector<uint8_t> getBytes() const override {
		if (range == XnFSet::F5toF8)
			return {0xE4, 0x21, loco.hi(), loco.lo(), static_cast<uint8_t>(fb.all & 0xF)};
		else
			return {0xE4, 0x22, loco.hi(), loco.lo(), static_cast<uint8_t>(fb.all >> 4)};
	}
	QString msg() const override {
		return "Set loco " + QString::number(loco.addr) + " func B: " + QString::number(fb.all, 2);
	}
};

enum class XnReadCVStatus {
	Ok = 0x14,
	ShortCircuit = 0x12,
	DataByteNotFound = 0x13,
	CSbusy = 0x1F,
	CSready = 0x11,
};

using XnReadCV =
    std::function<void(void *sender, XnReadCVStatus status, uint8_t cv, uint8_t value)>;

struct XnCmdReadDirect : public XnCmd {
	const uint8_t cv;
	XnReadCV const callback;

	XnCmdReadDirect(const uint8_t cv, XnReadCV const callback) : cv(cv), callback(callback) {}

	std::vector<uint8_t> getBytes() const override { return {0x22, 0x15, cv}; }
	QString msg() const override {
		return "Direct Mode CV " + QString::number(cv) + " read request";
	}
};

struct XnCmdRequestReadResult : public XnCmd {
	const uint8_t cv;
	XnReadCV const callback;

	XnCmdRequestReadResult(const uint8_t cv, XnReadCV const callback)
	    : cv(cv), callback(callback) {}

	std::vector<uint8_t> getBytes() const override { return {0x21, 0x10}; }
	QString msg() const override { return "Request for service mode results"; }
};

} // namespace Xn

#endif
