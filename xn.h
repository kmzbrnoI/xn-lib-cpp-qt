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
#include <QSerialPort>
#include <QTimer>
#include <memory>
#include <queue>
#include <vector>

#include "q-str-exception.h"
#include "xn-typedefs.h"

#define XN_VERSION_MAJOR 1
#define XN_VERSION_MINOR 1

namespace Xn {

constexpr size_t _MAX_HISTORY_LEN = 32;
constexpr size_t _HIST_CHECK_INTERVAL = 100; // ms
constexpr size_t _HIST_TIMEOUT = 1000; // ms
constexpr size_t _HIST_PROG_TIMEOUT = 10000; // 10 s
constexpr size_t _HIST_SEND_MAX = 3;
constexpr size_t _BUF_IN_TIMEOUT = 300; // ms
constexpr size_t _STEPS_CNT = 28;

struct EOpenError : public QStrException {
	EOpenError(const QString str) : QStrException(str) {}
};
struct EWriteError : public QStrException {
	EWriteError(const QString str) : QStrException(str) {}
};

enum class XnRecvCmdType {
	LiError = 0x01,
	LiVersion = 0x02,
	LiSettings = 0xF2,
	CsGeneralEvent = 0x61,
	CsStatus = 0x62,
	CsX63 = 0x63,
	CsLocoInfo = 0xE4,
};

class XpressNet : public QObject {
	Q_OBJECT

public:
	XnLogLevel loglevel = XnLogLevel::None;
	static constexpr unsigned _VERSION_MAJOR = XN_VERSION_MAJOR;
	static constexpr unsigned _VERSION_MINOR = XN_VERSION_MINOR;

	XpressNet(QObject *parent = nullptr);

	void connect(const QString &portname, uint32_t br, QSerialPort::FlowControl fc);
	void disconnect();
	bool connected() const;

	XnTrkStatus getTrkStatus() const;

	void setTrkStatus(const XnTrkStatus, UPXnCb ok = nullptr, UPXnCb err = nullptr);
	void emergencyStop(const LocoAddr, UPXnCb ok = nullptr, UPXnCb err = nullptr);
	void emergencyStop(UPXnCb ok = nullptr, UPXnCb err = nullptr);

	void getCommandStationVersion(XnGotCSVersion const &, UPXnCb err = nullptr);
	void getCommandStationStatus(UPXnCb ok = nullptr, UPXnCb err = nullptr);
	void getLIVersion(XnGotLIVersion const &, UPXnCb err = nullptr);
	void getLIAddress(XnGotLIAddress const &, UPXnCb err = nullptr);
	void setLIAddress(uint8_t addr, UPXnCb ok = nullptr, UPXnCb err = nullptr);

	void pomWriteCv(const LocoAddr, uint16_t cv, uint8_t value, UPXnCb ok = nullptr,
	                UPXnCb err = nullptr);
	void pomWriteBit(const LocoAddr, uint16_t cv, uint8_t biti, bool value, UPXnCb ok = nullptr,
	                 UPXnCb err = nullptr);
	void readCVdirect(const uint8_t cv, XnReadCV const &callback, UPXnCb err = nullptr);

	void setSpeed(const LocoAddr, uint8_t speed, XnDirection direction, UPXnCb ok = nullptr,
	              UPXnCb err = nullptr);
	void getLocoInfo(const LocoAddr, XnGotLocoInfo const &, UPXnCb err = nullptr);
	void setFuncA(const LocoAddr, const XnFA, UPXnCb ok = nullptr, UPXnCb err = nullptr);
	void setFuncB(const LocoAddr, const XnFB, const XnFSet, UPXnCb ok = nullptr,
	              UPXnCb err = nullptr);

	void accInfoRequest(const uint8_t groupAddr, const bool nibble,
	                    UPXnCb ok = nullptr, UPXnCb err = nullptr);
	void accOpRequest(const uint16_t portAddr, const bool state, // portAddr 0-2048
	                  UPXnCb ok = nullptr, UPXnCb err = nullptr);

	static QString xnReadCVStatusToQString(const XnReadCVStatus st);

private slots:
	void handleReadyRead();
	void handleError(QSerialPort::SerialPortError);
	void m_hist_timer_tick();
	void sp_about_to_close();

signals:
	void onError(QString error);
	void onLog(QString message, Xn::XnLogLevel loglevel);
	void onConnect();
	void onDisconnect();
	void onTrkStatusChanged(Xn::XnTrkStatus);

private:
	QSerialPort m_serialPort;
	QByteArray m_readData;
	QDateTime m_receiveTimeout;
	std::queue<XnHistoryItem> m_hist;
	QTimer m_hist_timer;
	XnTrkStatus m_trk_status = XnTrkStatus::Unknown;

	using MsgType = std::vector<uint8_t>;
	void parseMessage(MsgType &msg);
	void send(const MsgType);
	void send(std::unique_ptr<const XnCmd> &, UPXnCb ok = nullptr, UPXnCb err = nullptr);

	void handleMsgLiError(MsgType &msg);
	void handleMsgLiVersion(MsgType &msg);
	void handleMsgCsGeneralEvent(MsgType &msg);
	void handleMsgCsStatus(MsgType &msg);
	void handleMsgCsVersion(MsgType &msg);
	void handleMsgCvRead(MsgType &msg);
	void handleMsgLocoInfo(MsgType &msg);
	void handleMsgLIAddr(MsgType &msg);

	template <typename DataT>
	void send(const DataT &&, UPXnCb ok = nullptr, UPXnCb err = nullptr);

	void send(XnHistoryItem &&);

	void hist_ok();
	void hist_err();
	void hist_send();
	void log(const QString &message, const XnLogLevel loglevel);

	template <typename DataT>
	QString dataToStr(DataT, size_t len = 0);

	template <typename Target>
	bool is(const XnCmd *x);

	template <typename Target>
	bool is(const XnHistoryItem &h);
};

} // namespace Xn

#endif
