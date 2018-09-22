#ifndef _XN_TYPEDEFS_
#define _XN_TYPEDEFS_

#include <QDateTime>
#include <QByteArray>
#include <vector>

#include "../q-str-exception.h"

typedef void (*XnCommandCallback)(void* sender, void* data);

typedef enum class _xn_trk_status {
	UNKNOWN,
	OFF,
	ON,
	PROGRAMMING,
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
};

struct XnCmdOff : public XnCmd {
	std::vector<uint8_t> getBytes() { return {0x21, 0x80}; }
};

struct XnCmdOn : public XnCmd {
	std::vector<uint8_t> getBytes() { return {0x21, 0x81}; }
};

struct XnHistoryItem {
	XnHistoryItem(XnCmd& cmd, QDateTime last_sent, size_t no_sent,
	              XnCommandCallback* callback_err, XnCommandCallback* callback_ok)
		: cmd(cmd), last_sent(last_sent), no_sent(no_sent), callback_err(callback_err),
		  callback_ok(callback_ok) {}

	XnCmd& cmd;
	QDateTime last_sent;
	size_t no_sent;
	XnCommandCallback* callback_err;
	XnCommandCallback* callback_ok;
};

#endif
