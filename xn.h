#ifndef _XN_H_
#define _XN_H_

/* This file implements a low-level XpressNet class, which allows PC to
 * communicate with XpressNET command station over virtual serial port.
 */

#include <QSerialPort>
#include <QTimer>
#include <QDateTime>
#include <vector>
#include <queue>
#include <QTimer>
#include <memory>

#include "xn-typedefs.h"
#include "../q-str-exception.h"

namespace Xn {

const size_t _MAX_HISTORY_LEN = 32;
const size_t _HIST_CHECK_INTERVAL = 100; // ms
const size_t _HIST_TIMEOUT = 500; // ms
const size_t _HIST_SEND_MAX = 3;
const size_t _BUF_IN_TIMEOUT = 300; // ms

struct EOpenError : public QStrException {
	EOpenError(const QString str) : QStrException(str) {}
};
struct EWriteError : public QStrException {
	EWriteError(const QString str) : QStrException(str) {}
};

class XpressNet : public QObject {
	Q_OBJECT

public:
	XnLogLevel loglevel = XnLogLevel::None;

	XpressNet(QObject *parent = nullptr);

	void connect(QString portname, uint32_t br, QSerialPort::FlowControl fc);
	void disconnect();
	bool connected() const;

	XnTrkStatus getTrkStatus() const;

	void setTrkStatus(const XnTrkStatus, UPXnCb ok = nullptr, UPXnCb err = nullptr);
	void emergencyStop(const LocoAddr, UPXnCb ok = nullptr, UPXnCb err = nullptr);
	void emergencyStop(UPXnCb ok = nullptr, UPXnCb err = nullptr);
	void getCommandStationVersion(XnGotCSVersion const, UPXnCb err = nullptr);
	void getCommandStationStatus(UPXnCb ok = nullptr, UPXnCb err = nullptr);
	void getLIVersion(XnGotLIVersion const, UPXnCb err = nullptr);
	void getLIAddress(XnGotLIAddress const, UPXnCb err = nullptr);
	void setLIAddress(uint8_t addr, UPXnCb ok = nullptr, UPXnCb err = nullptr);
	void PomWriteCv(LocoAddr, uint16_t cv, uint8_t value, UPXnCb ok = nullptr,
	                UPXnCb err = nullptr);
	void setSpeed(LocoAddr, uint8_t speed, bool direction, UPXnCb ok = nullptr,
	              UPXnCb err = nullptr);

private slots:
	void handleReadyRead();
	void handleError(QSerialPort::SerialPortError);
	void m_hist_timer_tick();
	void sp_about_to_close();

signals:
	void onError(QString error);
	void onLog(QString message, XnLogLevel loglevel);
	void onConnect();
	void onDisconnect();
	void onTrkStatusChanged(XnTrkStatus);

private:
	QSerialPort m_serialPort;
	QByteArray m_readData;
	QDateTime m_receiveTimeout;
	std::queue<XnHistoryItem> m_hist;
	QTimer m_hist_timer;
	XnTrkStatus m_trk_status = XnTrkStatus::Unknown;

	void parseMessage(std::vector<uint8_t> msg);
	void send(const std::vector<uint8_t>);
	void send(std::unique_ptr<const XnCmd>&, UPXnCb ok = nullptr, UPXnCb err = nullptr);

	template<typename DataT>
	void send(const DataT&&, UPXnCb ok = nullptr, UPXnCb err = nullptr);

	void send(XnHistoryItem&&);

	void hist_ok();
	void hist_err();
	void hist_send();
	void log(const QString message, const XnLogLevel loglevel);

	template<typename DataT>
	QString dataToStr(DataT, size_t len = 0);
};

}//end namespace

#endif
