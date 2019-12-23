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
#include <QTimer>
#include <memory>
#include <queue>
#include <vector>

#include "q-str-exception.h"
#include "xn-commands.h"
#include "xn-loco-addr.h"

#define XN_VERSION_MAJOR 1
#define XN_VERSION_MINOR 2

namespace Xn {

constexpr size_t _MAX_HISTORY_LEN = 32;
constexpr size_t _HIST_CHECK_INTERVAL = 100; // ms
constexpr size_t _HIST_TIMEOUT = 1000; // ms
constexpr size_t _HIST_PROG_TIMEOUT = 10000; // 10 s
constexpr size_t _HIST_SEND_MAX = 3;
constexpr size_t _BUF_IN_TIMEOUT = 300; // ms
constexpr size_t _STEPS_CNT = 28;
constexpr size_t _MAX_HIST_BUF_COUNT = 3;

struct EOpenError : public QStrException {
	EOpenError(const QString str) : QStrException(str) {}
};
struct EWriteError : public QStrException {
	EWriteError(const QString str) : QStrException(str) {}
};
struct EInvalidTrkStatus : public QStrException {
	EInvalidTrkStatus(const QString str) : QStrException(str) {}
};

enum class LIType {
	LI100,
	LI101,
	uLI,
	LIUSBEth,
};

enum class TrkStatus {
	Unknown,
	Off,
	On,
	Programming,
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
	CsAccInfoResp = 0x42,
};

class XpressNet : public QObject {
	Q_OBJECT

public:
	LogLevel loglevel = LogLevel::None;
	static constexpr unsigned _VERSION_MAJOR = XN_VERSION_MAJOR;
	static constexpr unsigned _VERSION_MINOR = XN_VERSION_MINOR;

	XpressNet(QObject *parent = nullptr);

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

	void pomWriteCv(LocoAddr, uint16_t cv, uint8_t value, UPCb ok = nullptr,
	                UPCb err = nullptr);
	void pomWriteBit(LocoAddr, uint16_t cv, uint8_t biti, bool value, UPCb ok = nullptr,
	                 UPCb err = nullptr);
	void readCVdirect(uint8_t cv, ReadCV const &callback, UPCb err = nullptr);

	void setSpeed(LocoAddr, uint8_t speed, Direction direction, UPCb ok = nullptr,
	              UPCb err = nullptr);
	void getLocoInfo(LocoAddr, GotLocoInfo const &, UPCb err = nullptr);
	void setFuncA(LocoAddr, FA, UPCb ok = nullptr, UPCb err = nullptr);
	void setFuncB(LocoAddr, FB, FSet, UPCb ok = nullptr, UPCb err = nullptr);

	void accInfoRequest(uint8_t groupAddr, bool nibble, UPCb err = nullptr);
	void accOpRequest(uint16_t portAddr, bool state, // portAddr 0-2047
	                  UPCb ok = nullptr, UPCb err = nullptr);

	void histClear();

	static QString xnReadCVStatusToQString(ReadCVStatus st);
	LIType liType() const;

private slots:
	void handleReadyRead();
	void handleError(QSerialPort::SerialPortError);
	void m_hist_timer_tick();
	void sp_about_to_close();

signals:
	void onError(QString error);
	void onLog(QString message, Xn::LogLevel loglevel);
	void onConnect();
	void onDisconnect();
	void onTrkStatusChanged(Xn::TrkStatus);
	void onAccInputChanged(uint8_t groupAddr, bool nibble, bool error, Xn::FeedbackType inputType,
	                       Xn::AccInputsState state);

private:
	QSerialPort m_serialPort;
	QByteArray m_readData;
	QDateTime m_receiveTimeout;
	std::deque<HistoryItem> m_hist;
	std::queue<HistoryItem> m_out;
	QTimer m_hist_timer;
	TrkStatus m_trk_status = TrkStatus::Unknown;
	LIType m_liType;

	using MsgType = std::vector<uint8_t>;
	void parseMessage(MsgType &msg);
	void send(MsgType);
	void send(HistoryItem &&);
	void send(std::unique_ptr<const Cmd>, UPCb ok = nullptr, UPCb err = nullptr);
	void to_send(std::unique_ptr<const Cmd> &, UPCb ok = nullptr, UPCb err = nullptr);

	template <typename DataT>
	void to_send(const DataT &&, UPCb ok = nullptr, UPCb err = nullptr);

	void handleMsgLiError(MsgType &msg);
	void handleMsgLiVersion(MsgType &msg);
	void handleMsgCsGeneralEvent(MsgType &msg);
	void handleMsgCsStatus(MsgType &msg);
	void handleMsgCsVersion(MsgType &msg);
	void handleMsgCvRead(MsgType &msg);
	void handleMsgLocoInfo(MsgType &msg);
	void handleMsgLIAddr(MsgType &msg);
	void handleMsgAcc(MsgType &msg);

	void hist_ok();
	void hist_err();
	void hist_send();
	void send_next_out();
	void log(const QString &message, LogLevel loglevel);
	QDateTime timeout(const Cmd *x);
	bool liAcknowledgesSetAccState() const;

	template <typename DataT, typename ItemType>
	QString dataToStr(DataT, size_t len = 0);

	template <typename Target>
	bool is(const HistoryItem &h);
};

} // namespace Xn

#endif
