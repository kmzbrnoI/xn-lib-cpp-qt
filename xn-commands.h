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

struct Cmd {
	virtual std::vector<uint8_t> getBytes() const = 0;
	virtual QString msg() const = 0;
	virtual ~Cmd() = default;
	virtual bool conflict(const Cmd &) const { return false; }
};

template <typename Target>
bool is(const Cmd &x) {
	return (dynamic_cast<const Target *>(&x) != nullptr);
}

///////////////////////////////////////////////////////////////////////////////

struct CmdOff : public Cmd {
	std::vector<uint8_t> getBytes() const override { return {0x21, 0x80}; }
	QString msg() const override { return "Track Off"; }
};

struct CmdOn : public Cmd {
	std::vector<uint8_t> getBytes() const override { return {0x21, 0x81}; }
	QString msg() const override { return "Track On"; }
	bool conflict(const Cmd &cmd) const override { return is<CmdOff>(cmd); }
};

struct CmdEmergencyStop : public Cmd {
	std::vector<uint8_t> getBytes() const override { return {0x80}; }
	QString msg() const override { return "All Loco Emergency Stop"; }
};

struct CmdEmergencyStopLoco : public Cmd {
	const LocoAddr loco;

	CmdEmergencyStopLoco(const LocoAddr loco) : loco(loco) {}
	std::vector<uint8_t> getBytes() const override { return {0x92, loco.hi(), loco.lo()}; }
	QString msg() const override { return "Single Loco Emergency Stop : " + QString::number(loco); }
};

///////////////////////////////////////////////////////////////////////////////

using GotLIVersion = std::function<void(void *sender, unsigned hw, unsigned sw)>;

struct CmdGetLIVersion : public Cmd {
	GotLIVersion const callback;

	CmdGetLIVersion(GotLIVersion const callback) : callback(callback) {}
	std::vector<uint8_t> getBytes() const override { return {0xF0}; }
	QString msg() const override { return "LI Get Version"; }
};

using GotLIAddress = std::function<void(void *sender, unsigned addr)>;

struct CmdGetLIAddress : public Cmd {
	GotLIAddress const callback;

	CmdGetLIAddress(GotLIAddress const callback) : callback(callback) {}
	std::vector<uint8_t> getBytes() const override { return {0xF2, 0x01, 0x00}; }
	QString msg() const override { return "LI Get Address"; }
};

struct CmdSetLIAddress : public Cmd {
	const unsigned addr;

	CmdSetLIAddress(const unsigned addr) : addr(addr) {}
	std::vector<uint8_t> getBytes() const override {
		return {0xF2, 0x01, static_cast<uint8_t>(addr)};
	}
	QString msg() const override { return "LI Set Address to " + QString::number(addr); }
	bool conflict(const Cmd &cmd) const override { return is<CmdSetLIAddress>(cmd); }
};

using GotCSVersion = std::function<void(void *sender, unsigned major, unsigned minor, uint8_t id)>;

struct CmdGetCSVersion : public Cmd {
	GotCSVersion const callback;

	CmdGetCSVersion(GotCSVersion const callback) : callback(callback) {}
	std::vector<uint8_t> getBytes() const override { return {0x21, 0x21}; }
	QString msg() const override { return "Get Command station version"; }
};

struct CmdGetCSStatus : public Cmd {
	std::vector<uint8_t> getBytes() const override { return {0x21, 0x24}; }
	QString msg() const override { return "Get Command station status"; }
};

///////////////////////////////////////////////////////////////////////////////

struct CmdPomWriteCv : public Cmd {
	const LocoAddr loco;
	const uint16_t cv;
	const uint8_t value;

	CmdPomWriteCv(const LocoAddr loco, const uint16_t cv, const uint8_t value)
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
	bool conflict(const Cmd &cmd) const override {
		if (is<CmdPomWriteCv>(cmd)) {
			const auto &casted = dynamic_cast<const CmdPomWriteCv &>(cmd);
			return ((casted.loco == this->loco) && (casted.cv == this->cv));
		}
		return false;
	}
};

struct CmdPomWriteBit : public Cmd {
	const LocoAddr loco;
	const uint16_t cv;
	const uint8_t biti;
	const bool value;

	CmdPomWriteBit(const LocoAddr loco, const uint16_t cv, const unsigned biti, const bool value)
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
	bool conflict(const Cmd &cmd) const override {
		if (is<CmdPomWriteBit>(cmd)) {
			const auto &casted = dynamic_cast<const CmdPomWriteBit &>(cmd);
			return ((casted.loco == this->loco) && (casted.cv == this->cv) &&
			        (casted.biti == this->biti));
		}
		if (is<CmdPomWriteCv>(cmd)) {
			const auto &casted = dynamic_cast<const CmdPomWriteCv &>(cmd);
			return ((casted.loco == this->loco) && (casted.cv == this->cv));
		}
		return false;
	}
};

///////////////////////////////////////////////////////////////////////////////

union FA {
	uint8_t all;
	struct {
		bool f1 : 1;
		bool f2 : 1;
		bool f3 : 1;
		bool f4 : 1;
		bool f0 : 1;
		bool _ : 3;
	} sep;

	FA(uint8_t fa) : all(fa) {}
	FA() : all(0) {}
};

union FB {
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

	FB(uint8_t fb) : all(fb) {}
	FB() : all(0) {}
};

union FC {
	uint8_t all;
	struct {
		bool f13 : 1;
		bool f14 : 1;
		bool f15 : 1;
		bool f16 : 1;
		bool f17 : 1;
		bool f18 : 1;
		bool f19 : 1;
		bool f20 : 1;
	} sep;

	FC(uint8_t fc) : all(fc) {}
	FC() : all(0) {}
};

union FD {
	uint8_t all;
	struct {
		bool f21 : 1;
		bool f22 : 1;
		bool f23 : 1;
		bool f24 : 1;
		bool f25 : 1;
		bool f26 : 1;
		bool f27 : 1;
		bool f28 : 1;
	} sep;

	FD(uint8_t fd) : all(fd) {}
	FD() : all(0) {}
};

enum class Direction {
	Backward = false,
	Forward = true,
};

using GotLocoInfo = std::function<void(void *sender, bool used, Direction direction,
                                         unsigned speed, FA fa, FB fb)>;

struct CmdGetLocoInfo : public Cmd {
	const LocoAddr loco;
	GotLocoInfo const callback;

	CmdGetLocoInfo(const LocoAddr loco, GotLocoInfo const callback)
	    : loco(loco), callback(callback) {}
	std::vector<uint8_t> getBytes() const override { return {0xE3, 0x00, loco.hi(), loco.lo()}; }
	QString msg() const override { return "Get Loco Information " + QString::number(loco.addr); }
};

using GotLocoFunc1328 = std::function<void(void *sender, FC fc, FD fd)>;

struct CmdGetLocoFunc1328 : public Cmd {
	const LocoAddr loco;
	GotLocoFunc1328 const callback;

	CmdGetLocoFunc1328(const LocoAddr loco, GotLocoFunc1328 const callback)
	    : loco(loco), callback(callback) {}
	std::vector<uint8_t> getBytes() const override { return {0xE3, 0x09, loco.hi(), loco.lo()}; }
	QString msg() const override {
		return "Get Loco Function 13-28 Status " + QString::number(loco.addr);
	}
};
///////////////////////////////////////////////////////////////////////////////

struct CmdSetSpeedDir : public Cmd {
	const LocoAddr loco;
	const unsigned speed;
	const Direction dir;

	CmdSetSpeedDir(const LocoAddr loco, const unsigned speed, const Direction dir)
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
	bool conflict(const Cmd &cmd) const override {
		if (is<CmdSetSpeedDir>(cmd)) {
			const auto &casted = dynamic_cast<const CmdSetSpeedDir &>(cmd);
			return (casted.loco == this->loco);
		}
		if (is<CmdEmergencyStop>(cmd))
			return true;
		if (is<CmdEmergencyStopLoco>(cmd)) {
			const auto &casted = dynamic_cast<const CmdEmergencyStopLoco &>(cmd);
			return (casted.loco == this->loco);
		}
		return false;
	}
};

///////////////////////////////////////////////////////////////////////////////

struct CmdSetFuncA : public Cmd {
	const LocoAddr loco;
	const FA fa;

	CmdSetFuncA(const LocoAddr loco, const FA fa) : loco(loco), fa(fa) {}
	std::vector<uint8_t> getBytes() const override {
		return {0xE4, 0x20, loco.hi(), loco.lo(), fa.all};
	}
	QString msg() const override {
		return "Set loco " + QString::number(loco.addr) + " func A (0-4): " + QString::number(fa.all, 2).rightJustified(5, '0');
	}
	bool conflict(const Cmd &cmd) const override {
		if (is<CmdSetFuncA>(cmd)) {
			const auto &casted = dynamic_cast<const CmdSetFuncA &>(cmd);
			return (casted.loco == this->loco);
		}
		return false;
	}
};

enum class FSet {
	F5toF8,
	F9toF12,
};

struct CmdSetFuncB : public Cmd {
	const LocoAddr loco;
	const FB fb;
	const FSet range;

	CmdSetFuncB(const LocoAddr loco, const FB fb, const FSet range)
	    : loco(loco), fb(fb), range(range) {}
	std::vector<uint8_t> getBytes() const override {
		if (range == FSet::F5toF8)
			return {0xE4, 0x21, loco.hi(), loco.lo(), static_cast<uint8_t>(fb.all & 0xF)};
		return {0xE4, 0x22, loco.hi(), loco.lo(), static_cast<uint8_t>(fb.all >> 4)};
	}
	QString msg() const override {
		return "Set loco " + QString::number(loco.addr) + " func B (5-12): " + QString::number(fb.all, 2).rightJustified(8, '0');
	}
	bool conflict(const Cmd &cmd) const override {
		if (is<CmdSetFuncB>(cmd)) {
			const auto &casted = dynamic_cast<const CmdSetFuncB &>(cmd);
			return ((casted.loco == this->loco) && (casted.range == this->range));
		}
		return false;
	}
};

struct CmdSetFuncC : public Cmd {
	const LocoAddr loco;
	const FC fc;

	CmdSetFuncC(const LocoAddr loco, const FC fc) : loco(loco), fc(fc) {}
	std::vector<uint8_t> getBytes() const override {
		return {0xE4, 0x23, loco.hi(), loco.lo(), fc.all};
	}
	QString msg() const override {
		return "Set loco " + QString::number(loco.addr) + " func C (13-20): " + QString::number(fc.all, 2).rightJustified(8, '0');
	}
	bool conflict(const Cmd &cmd) const override {
		if (is<CmdSetFuncC>(cmd)) {
			const auto &casted = dynamic_cast<const CmdSetFuncC &>(cmd);
			return (casted.loco == this->loco);
		}
		return false;
	}
};

struct CmdSetFuncD : public Cmd {
	const LocoAddr loco;
	const FD fd;

	CmdSetFuncD(const LocoAddr loco, const FD fd) : loco(loco), fd(fd) {}
	std::vector<uint8_t> getBytes() const override {
		return {0xE4, 0x28, loco.hi(), loco.lo(), fd.all};
	}
	QString msg() const override {
		return "Set loco " + QString::number(loco.addr) + " func D (21-28): " + QString::number(fd.all, 2).rightJustified(8, '0');
	}
	bool conflict(const Cmd &cmd) const override {
		if (is<CmdSetFuncD>(cmd)) {
			const auto &casted = dynamic_cast<const CmdSetFuncD &>(cmd);
			return (casted.loco == this->loco);
		}
		return false;
	}
};

///////////////////////////////////////////////////////////////////////////////

enum class ReadCVStatus {
	Ok = 0x14,
	ShortCircuit = 0x12,
	DataByteNotFound = 0x13,
	CSbusy = 0x1F,
	CSready = 0x11,
};

using ReadCV =
    std::function<void(void *sender, ReadCVStatus status, uint8_t cv, uint8_t value)>;

struct CmdReadDirect : public Cmd {
	const uint8_t cv;
	ReadCV const callback;

	CmdReadDirect(const uint8_t cv, ReadCV const callback) : cv(cv), callback(callback) {}

	std::vector<uint8_t> getBytes() const override { return {0x22, 0x15, cv}; }
	QString msg() const override {
		return "Direct Mode CV " + QString::number(cv) + " read request";
	}
};

struct CmdRequestReadResult : public Cmd {
	const uint8_t cv;
	ReadCV const callback;

	CmdRequestReadResult(const uint8_t cv, ReadCV const callback)
	    : cv(cv), callback(callback) {}

	std::vector<uint8_t> getBytes() const override { return {0x21, 0x10}; }
	QString msg() const override { return "Request for service mode results"; }
};

///////////////////////////////////////////////////////////////////////////////

struct CmdAccInfoRequest : public Cmd {
	const uint8_t groupAddr;
	const bool nibble;

	CmdAccInfoRequest(const uint8_t groupAddr, const bool nibble)
	    : groupAddr(groupAddr), nibble(nibble) {}
	std::vector<uint8_t> getBytes() const override {
		return {0x42, groupAddr, static_cast<uint8_t>(0x80+nibble) };
	}
	QString msg() const override {
		return "Accessory Decoder Information Request: group " + QString::number(groupAddr) +
		       ", nibble:" + QString::number(nibble);
	}
};

struct CmdAccOpRequest : public Cmd {
	const uint16_t portAddr; // 0-2047
	const bool state;

	CmdAccOpRequest(const uint16_t portAddr, const bool state)
	    : portAddr(portAddr), state(state) {}
	std::vector<uint8_t> getBytes() const override {
		return {
			0x52,
			static_cast<uint8_t>(portAddr >> 3),
			static_cast<uint8_t>(0x80 + (portAddr & 0x7) + (state << 3)) };
	}
	QString msg() const override {
		return "Accessory Decoder Operation Request: port " + QString::number(portAddr) +
		       ", state:" + QString::number(state);
	}
	bool conflict(const Cmd &cmd) const override {
		if (is<CmdAccOpRequest>(cmd)) {
			const auto &casted = dynamic_cast<const CmdAccOpRequest &>(cmd);
			return ((casted.portAddr/2) == (this->portAddr/2));
		}
		return false;
	}
};

///////////////////////////////////////////////////////////////////////////////

} // namespace Xn

#endif
