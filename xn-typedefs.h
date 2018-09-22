#ifndef _XN_TYPEDEFS_
#define _XN_TYPEDEFS_

#include <QDateTime>
#include <QByteArray>
#include <vector>

typedef void (*XnCommandCallback)(void* sender, void* data);

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
