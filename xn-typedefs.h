#ifndef _XN_TYPEDEFS_
#define _XN_TYPEDEFS_

#include <QDateTime>
#include <QByteArray>
#include <vector>

#include "../q-str-exception.h"

typedef void (*XnCommandCallbackFunc)(void* sender, void* data);

typedef struct {
	XnCommandCallbackFunc func;
	void* data;
} XnCommandCallback;

typedef enum class _xn_trk_status {
	Unknown,
	Off,
	On,
	Programming,
} XnTrkStatus;

class EInvalidAddr : public QStrException {
public:
	EInvalidAddr(const QString str) : QStrException(str) {}
};

struct LocoAddr {
	uint16_t addr;

	LocoAddr(uint16_t addr) : addr(addr) {
		if (addr > 9999)
			throw EInvalidAddr("Invalid loco address!");
	}

	LocoAddr(uint8_t lo, uint8_t hi) : LocoAddr(lo + ((hi-0xC0) << 8)) {}

	uint8_t lo() { return addr & 0xFF; }
	uint8_t hi() { return ((addr >> 8) & 0xFF) + 0xC0; }
};

struct XnCmd {
	virtual std::vector<uint8_t> getBytes() = 0;
	virtual QString msg() = 0;
};

struct XnCmdOff : public XnCmd {
	std::vector<uint8_t> getBytes() override { return {0x21, 0x80}; }
	QString msg() override { return "Track Off"; }
};

struct XnCmdOn : public XnCmd {
	std::vector<uint8_t> getBytes() override { return {0x21, 0x81}; }
	QString msg() override { return "Track On"; }
};

struct XnHistoryItem {
	XnHistoryItem(XnCmd& cmd, QDateTime timeout, size_t no_sent,
	              XnCommandCallback* callback_err, XnCommandCallback* callback_ok)
		: cmd(cmd), timeout(timeout), no_sent(no_sent), callback_err(callback_err),
		  callback_ok(callback_ok) {}

	XnCmd& cmd;
	QDateTime timeout;
	size_t no_sent;
	XnCommandCallback* callback_err;
	XnCommandCallback* callback_ok;
};

typedef enum class _xn_log_level {
	None = 0,
	Error = 1,
	Warning = 2,
	Info = 3,
	Data = 4,
	Debug = 5,
} XnLogLevel;

#endif
