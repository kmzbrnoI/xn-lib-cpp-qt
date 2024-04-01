#ifndef XN_H
#define XN_H

/*
This file implements a low-level XpressNet class, which allows PC to
communicate with XpressNET command station over virtual serial port.

A command from PC to Command station is send by calling any of the public
functions listed below. You may pass an 'ok' and an 'error' callback when
calling this function. Exactly one of these callbacks is *guaranteed* to be
called based on the response from command station or LI.

How does sending work?
 (1) User calls a function.
 (2) Data are sent to the asynchronous serial port.
 (3) The function ends.
 (4a) When the command station sends a proper reply, 'ok' callback is called.
 (4b) When the command station sends no reply or improper reply, the command
      is sent again. Iff the command station does not reply for _HIST_SEND_MAX
      times (3), 'error' callback is called.

 * Response to the user`s command is transmitted to the user as callback.
 * General callbacks are implemented as slots (see below).
 * For adding more commands, see xn-typedefs.h.
*/

#include <QDateTime>
#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <memory>
#include <queue>
#include <vector>

#include "q-str-exception.h"
#include "xn-commands.h"
#include "xn-loco-addr.h"

#define XN_VERSION_MAJOR 2
#define XN_VERSION_MINOR 7

namespace Xn {

constexpr size_t _MAX_HISTORY_LEN = 32;
constexpr size_t _HIST_CHECK_INTERVAL = 100; // ms
constexpr size_t _HIST_TIMEOUT = 1000; // ms
constexpr size_t _HIST_PROG_TIMEOUT = 10000; // 10 s
constexpr size_t _HIST_SEND_MAX = 3;
constexpr size_t _BUF_IN_TIMEOUT = 300; // ms
constexpr size_t _STEPS_CNT = 28;
constexpr size_t _MAX_HIST_BUF_COUNT = 3;

constexpr size_t _OUT_TIMER_INTERVAL_DEFAULT = 50; // ms
constexpr size_t _OUT_TIMER_INTERVAL_MIN = 50; // ms
constexpr size_t _OUT_TIMER_INTERVAL_MAX = 500; // ms

struct EOpenError : public QStrException {
	EOpenError(const QString str) : QStrException(str) {}
};
struct EWriteError : public QStrException {
	EWriteError(const QString str) : QStrException(str) {}
};
struct EInvalidTrkStatus : public QStrException {
	EInvalidTrkStatus(const QString str) : QStrException(str) {}
};
struct EUnsupportedInterface : public QStrException {
	EUnsupportedInterface(const QString str) : QStrException(str) {}
};
struct EInvalidConfig : public QStrException {
	EInvalidConfig(const QString str) : QStrException(str) {}
};

enum class LIType {
	LI100,
	LI101,
	uLI,
	LIUSBEth,
};

LIType liInterface(const QString &name);
QString liInterfaceName(const LIType &type);

enum class TrkStatus {
	Unknown = 0,
	Off = 1,
	On = 2,
	Programming = 3,
};

using CommandCallbackFunc = std::function<void(void *sender, void *data)>;

struct CommandCallback {
	CommandCallbackFunc const func;
	void *const data;

	CommandCallback(CommandCallbackFunc const func, void *const data = nullptr)
	    : func(func), data(data) {}
};

using Cb = CommandCallback;
using UPCb = std::unique_ptr<CommandCallback>;

struct HistoryItem {
	HistoryItem(std::unique_ptr<const Cmd> &cmd, QDateTime timeout, size_t no_sent,
	            std::unique_ptr<Cb> &&callback_ok, std::unique_ptr<Cb> &&callback_err)
	    : cmd(std::move(cmd))
	    , timeout(timeout)
	    , no_sent(no_sent)
	    , callback_ok(std::move(callback_ok))
	    , callback_err(std::move(callback_err)) {}
	HistoryItem(HistoryItem &&hist) noexcept
	    : cmd(std::move(hist.cmd))
	    , timeout(hist.timeout)
	    , no_sent(hist.no_sent)
	    , callback_ok(std::move(hist.callback_ok))
	    , callback_err(std::move(hist.callback_err)) {}

	std::unique_ptr<const Cmd> cmd;
	QDateTime timeout;
	size_t no_sent;
	std::unique_ptr<Cb> callback_ok;
	std::unique_ptr<Cb> callback_err;
};

enum class LogLevel {
	None = 0,
	Error = 1,
	Warning = 2,
	Info = 3,
	Commands = 4,
	RawData = 5,
	Debug = 6,
};

enum class FeedbackType {
	accWithoutFb = 0,
	accWithFb = 1,
	fb = 2,
	reserved = 3,
};

union AccInputsState {
	uint8_t all;
	struct {
		bool i0 : 1;
		bool i1 : 1;
		bool i2 : 1;
		bool i3 : 1;
	} sep;
};

enum class RecvCmdType {
	LiError = 0x01,
	LiVersion = 0x02,
	LiSettings = 0xF2,
	CsGeneralEvent = 0x61,
	CsStatus = 0x62,
	CsX63 = 0x63,
	CsLocoInfo = 0xE4,
	CsLocoFunc = 0xE3,
	CsAccInfoResp = 0x42,
};

struct XNConfig {
	size_t outInterval = _OUT_TIMER_INTERVAL_DEFAULT;
};

class XpressNet : public QObject {
	Q_OBJECT

public:
	LogLevel loglevel = LogLevel::None;
	static constexpr unsigned _VERSION_MAJOR = XN_VERSION_MAJOR;
	static constexpr unsigned _VERSION_MINOR = XN_VERSION_MINOR;

	XpressNet(QObject *parent = nullptr);
	~XpressNet() override;

	void connect(const QString &portname, int32_t br, QSerialPort::FlowControl fc, LIType liType);
	void disconnect();
	bool connected() const;

	TrkStatus getTrkStatus() const;

	void setTrkStatus(TrkStatus, UPCb ok = nullptr, UPCb err = nullptr);
	void emergencyStop(LocoAddr, UPCb ok = nullptr, UPCb err = nullptr);
	void emergencyStop(UPCb ok = nullptr, UPCb err = nullptr);

	void getCommandStationVersion(GotCSVersion const &, UPCb err = nullptr);
	void getCommandStationStatus(UPCb ok = nullptr, UPCb err = nullptr);
	void getLIVersion(GotLIVersion const &, UPCb err = nullptr);
	void getLIAddress(GotLIAddress const &, UPCb err = nullptr);
	void setLIAddress(uint8_t addr, UPCb ok = nullptr, UPCb err = nullptr);

	void pomWriteCv(LocoAddr, uint16_t cv, uint8_t value, UPCb ok = nullptr, UPCb err = nullptr);
	void pomWriteBit(LocoAddr, uint16_t cv, uint8_t biti, bool value, UPCb ok = nullptr,
	                 UPCb err = nullptr);
	void readCVdirect(uint8_t cv, ReadCV const &callback, UPCb err = nullptr);
	void writeCVdirect(uint8_t cv, uint8_t value, UPCb ok = nullptr, UPCb err = nullptr);

	void setSpeed(LocoAddr, uint8_t speed, Direction direction, UPCb ok = nullptr,
	              UPCb err = nullptr);
	void getLocoInfo(LocoAddr, GotLocoInfo const &, UPCb err = nullptr);
	void getLocoFunc1328(LocoAddr, GotLocoFunc1328, UPCb err = nullptr);
	void setFuncA(LocoAddr, FA, UPCb ok = nullptr, UPCb err = nullptr);
	void setFuncB(LocoAddr, FB, FSet, UPCb ok = nullptr, UPCb err = nullptr);
	void setFuncC(LocoAddr, FC, UPCb ok = nullptr, UPCb err = nullptr);
	void setFuncD(LocoAddr, FD, UPCb ok = nullptr, UPCb err = nullptr);

	void accInfoRequest(uint8_t groupAddr, bool nibble, UPCb err = nullptr);
	void accOpRequest(uint16_t portAddr, bool state, // portAddr 0-2047
	                  UPCb ok = nullptr, UPCb err = nullptr);

	void histClear();

	static QString xnReadCVStatusToQString(ReadCVStatus st);
	static std::vector<QSerialPortInfo> ports(LIType);
	LIType liType() const;

	XNConfig config() const;
	void setConfig(XNConfig config);

private slots:
	void handleReadyRead();
	void handleError(QSerialPort::SerialPortError);
	void m_hist_timer_tick();
	void m_out_timer_tick();
	void sp_about_to_close();

signals:
	void onError(QString error);
	void onLog(QString message, Xn::LogLevel loglevel);
	void onConnect();
	void onDisconnect();
	void onTrkStatusChanged(Xn::TrkStatus);
	void onLocoStolen(Xn::LocoAddr);
	void onAccInputChanged(uint8_t groupAddr, bool nibble, bool error, Xn::FeedbackType inputType,
	                       Xn::AccInputsState state);

private:
	QSerialPort m_serialPort;
	QByteArray m_readData;
	QDateTime m_receiveTimeout;
	QDateTime m_lastSent;
	std::deque<HistoryItem> m_hist;
	std::deque<HistoryItem> m_out;
	QTimer m_hist_timer;
	QTimer m_out_timer;
	TrkStatus m_trk_status = TrkStatus::Unknown;
	LIType m_liType;
	XNConfig m_config;

	using MsgType = std::vector<uint8_t>;
	void parseMessage(MsgType &msg);
	void send(MsgType);
	void send(std::unique_ptr<const Cmd>, UPCb ok = nullptr, UPCb err = nullptr,
	          size_t no_sent = 1);
	void to_send(HistoryItem &&, bool bypass_m_out_emptiness = false);
	void to_send(std::unique_ptr<const Cmd> &, UPCb ok = nullptr, UPCb err = nullptr,
	             size_t no_sent = 1, bool bypass_m_out_emptiness = false);

	template <typename T>
	void to_send(const T &&cmd, UPCb ok = nullptr, UPCb err = nullptr);

	void handleMsgLiError(MsgType &msg);
	void handleMsgLiVersion(MsgType &msg);
	void handleMsgCsGeneralEvent(MsgType &msg);
	void handleMsgCsStatus(MsgType &msg);
	void handleMsgCsVersion(MsgType &msg);
	void handleMsgCvRead(MsgType &msg);
	void handleMsgLocoInfo(MsgType &msg);
	void handleMsgLocoFunc(MsgType &msg);
	void handleMsgLIAddr(MsgType &msg);
	void handleMsgAcc(MsgType &msg);

	void hist_ok();
	void hist_err(bool _log = true);
	void hist_send();
	void send_next_out();
	void log(const QString &message, LogLevel loglevel);
	QDateTime timeout(const Cmd *x);
	bool liAcknowledgesSetAccState() const;
	bool conflictWithHistory(const Cmd &) const;
	bool conflictWithOut(const Cmd &) const;

	template <typename DataT, typename ItemType>
	QString dataToStr(DataT, size_t len = 0);

	template <typename Target>
	bool is(const HistoryItem &h);
};

// Templated functions must be in header file to compile

template <typename T>
void XpressNet::to_send(const T &&cmd, UPCb ok, UPCb err) {
	std::unique_ptr<const Cmd> cmd2(std::make_unique<const T>(cmd));
	to_send(cmd2, std::move(ok), std::move(err));
}

template <typename DataT, typename ItemType>
QString XpressNet::dataToStr(DataT data, size_t len) {
	QString out;
	size_t i = 0;
	for (auto d = data.begin(); (d != data.end() && (len == 0 || i < len)); d++, i++)
		out += "0x" +
		       QString("%1 ").arg(static_cast<ItemType>(*d), 2, 16, QLatin1Char('0')).toUpper();
	return out.trimmed();
}

template <typename Target>
bool XpressNet::is(const HistoryItem &h) {
	return (dynamic_cast<const Target *>(h.cmd.get()) != nullptr);
}

QString flowControlToStr(QSerialPort::FlowControl);

} // namespace Xn

#endif
