#ifndef _XN_H_
#define _XN_H_

/* This file implemens a low-level class XpressNet, wchich allow PC to
 * communicate with XpressNET command station over virtual serial port.
 */

#include <QSerialPort>
#include <QTimer>
#include <QDateTime>
#include <vector>
#include <queue>

#include "xn-typedefs.h"
#include "../q-str-exception.h"

const size_t _MAX_HISTORY_LEN = 32;
const size_t _HIST_CHECK_INTERVAL = 100; // ms
const size_t _HIST_TIMEOUT = 500; // ms
const size_t _HIST_SEND_MAX = 3;
const size_t _BUF_IN_TIMEOUT = 300; // ms

class EOpenError : public QStrException {
public:
	EOpenError(const QString str) : QStrException(str) {}
};

class XpressNet : public QObject {
	Q_OBJECT

public:
	XpressNet(QString portname);

	void setTrkStatus(XnTrkStatus);
	void emergencyStop(LocoAddr);
	void emergencyStop();
	void getCommandStationVersion();
	void getLIVersion();
	void getLIAddress();
	void setLIAddress(uint8_t addr);
	void PomWriteCv(LocoAddr, uint8_t cv, uint8_t value);
	void setSpeed(LocoAddr, uint8_t speed, bool direction);

private slots:
	void handleReadyRead();
	void handleError(QSerialPort::SerialPortError);

private:
	QSerialPort m_serialPort;
	QByteArray m_readData;
	QDateTime m_receiveTimeout;
	std::queue<XnHistoryItem> m_hist;

	void parseMessage(std::vector<uint8_t> msg);
	void send(std::vector<uint8_t>);
};

#endif
