#ifndef XN_TYPEDEFS
#define XN_TYPEDEFS

/*
This file defines types for XpressNET library.
Locomotive address is represented as an instance of LocoAddr class.
See xn.h or README for more documentation.
*/

#include <QByteArray>
#include <QDateTime>
#include <memory>

#include "q-str-exception.h"
#include "xn-commands.h"
#include "xn-loco-addr.h"

namespace Xn {

struct EInvalidTrkStatus : public QStrException {
	EInvalidTrkStatus(const QString str) : QStrException(str) {}
};

enum class XnTrkStatus {
	Unknown,
	Off,
	On,
	Programming,
};

using XnCommandCallbackFunc = std::function<void(void *sender, void *data)>;

struct XnCommandCallback {
	XnCommandCallbackFunc const func;
	void *const data;

	XnCommandCallback(XnCommandCallbackFunc const func, void *const data = nullptr)
	    : func(func), data(data) {}
};

using XnCb = XnCommandCallback;
using UPXnCb = std::unique_ptr<XnCommandCallback>;

struct XnHistoryItem {
	XnHistoryItem(std::unique_ptr<const XnCmd> &cmd, QDateTime timeout, size_t no_sent,
	              std::unique_ptr<XnCb> &&callback_ok, std::unique_ptr<XnCb> &&callback_err)
	    : cmd(std::move(cmd))
	    , timeout(timeout)
	    , no_sent(no_sent)
	    , callback_ok(std::move(callback_ok))
	    , callback_err(std::move(callback_err)) {}
	XnHistoryItem(XnHistoryItem &&hist) noexcept
	    : cmd(std::move(hist.cmd))
	    , timeout(hist.timeout)
	    , no_sent(hist.no_sent)
	    , callback_ok(std::move(hist.callback_ok))
	    , callback_err(std::move(hist.callback_err)) {}

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

} // namespace Xn

#endif
