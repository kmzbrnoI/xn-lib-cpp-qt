#include <sstream>

#include "xn.h"


XpressNet::XpressNet(QString portname, uint32_t br, QSerialPort::FlowControl fc,
                     QObject *parent)
	: QObject(parent) {
	m_serialPort.setBaudRate(br);
	m_serialPort.setFlowControl(fc);
	m_serialPort.setPortName(portname);
	m_serialPort.setReadBufferSize(256);

	if (!m_serialPort.open(QIODevice::ReadWrite))
		throw EOpenError(m_serialPort.errorString());

	QObject::connect(&m_hist_timer, SIGNAL(timeout()), this, SLOT(m_hist_timer_tick()));
	m_hist_timer.start(_HIST_CHECK_INTERVAL);
}

void XpressNet::send(const std::vector<uint8_t> data) {
	QByteArray qdata(reinterpret_cast<const char*>(data.data()), data.size());

	uint8_t x = 0;
	for (uint8_t d : data)
		x ^= d;
	qdata.append(x);

	log("PUT: " + dataToStr(qdata), XnLogLevel::Data);

	int sent = m_serialPort.write(qdata);

	if (sent == -1) {
		// TODO
	}
	if (sent != qdata.size()) {
		// TODO
	}
}

void XpressNet::send(const XnCmd& cmd, CPXnCb ok, CPXnCb err) {
	XnHistoryItem hist(cmd, QDateTime::currentDateTime(), 1, ok, err);
	m_hist.push(hist);

	log("PUT: " + cmd.msg(), XnLogLevel::Info);
	log("PUT: " + dataToStr(cmd.getBytes()), XnLogLevel::Data);
	send(cmd.getBytes());
}

void XpressNet::send(const XnCmd&& cmd, CPXnCb ok, CPXnCb err) {
	const XnCmd& cmd2(cmd);
	send(cmd2, ok, err);
}

void XpressNet::send(XnHistoryItem& hist) {
	hist.no_sent++;
	hist.timeout = QDateTime::currentDateTime().addMSecs(_HIST_TIMEOUT);
	m_hist.push(hist);

	log("PUT: " + hist.cmd.msg(), XnLogLevel::Info);
	log("PUT: " + dataToStr(hist.cmd.getBytes()), XnLogLevel::Data);
	send(hist.cmd.getBytes());
}

void XpressNet::handleReadyRead() {
	// check timeout
	if (m_receiveTimeout < QDateTime::currentDateTime() && m_readData.size() > 0) {
		// clear input buffer when data not received for a long time
		m_readData.clear();
	}

	m_readData.append(m_serialPort.readAll());
	m_receiveTimeout = QDateTime::currentDateTime().addMSecs(_BUF_IN_TIMEOUT);

	while (m_readData.size() > 0 && m_readData.size() >= (m_readData[0] & 0x0F)+2) {
		unsigned int length = (m_readData[0] & 0x0F)+2; // including header byte & xor
		uint8_t x = 0;
		for (uint i = 0; i < length; i++)
			x ^= m_readData[i];

		log("GET: " + dataToStr(m_readData, length), XnLogLevel::Data);

		if (x != 0) {
			// XOR error
			log("XOR error: " + dataToStr(m_readData, length), XnLogLevel::Warning);
			m_readData.remove(0, static_cast<int>(length));
			continue;
		}

		std::vector<uint8_t> message(m_readData.begin(), m_readData.begin() + length);
		parseMessage(message);
		m_readData.remove(0, static_cast<int>(length));
	}
}

void XpressNet::handleError(QSerialPort::SerialPortError serialPortError) {
	(void)serialPortError;
	onError(m_serialPort.errorString());
}

void XpressNet::parseMessage(std::vector<uint8_t> msg) {
	if (0x01 == msg[0]) {
		if (0x01 == msg[1]) {
			log("GET: Error occurred between the interfaces and the PC", XnLogLevel::Error);
		} else if (0x02 == msg[1]) {
			log("GET: Error occurred between the interfaces and the command station", XnLogLevel::Error);
		} else if (0x03 == msg[1]) {
			log("GET: Unknown communication error", XnLogLevel::Error);
		} else if (0x04 == msg[1]) {
			log("GET: OK", XnLogLevel::Info);
			hist_ok();
		} else if (0x05 == msg[1]) {
			log("GET: GET: The Command Station is no longer providing the LI "\
			    "a timeslot for communication", XnLogLevel::Error);
		} else if (0x06 == msg[1]) {
			log("GET: GET: Buffer overflow in the LI", XnLogLevel::Error);
		}
	} else if (0x02 == msg[0]) {
		// Got LI version
		unsigned hw = (msg[1] & 0x0F) + 10*(msg[1] >> 4);
		unsigned sw = (msg[2] & 0x0F) + 10*(msg[2] >> 4);

		log("GET: LI version; HW: " + QString(hw) + ", SW: " + QString(sw), XnLogLevel::Info);

		if (dynamic_cast<const XnCmdGetLIVersion*>(&m_hist.front().cmd) != nullptr) {
			hist_ok();
			dynamic_cast<const XnCmdGetLIVersion&>(m_hist.front().cmd).callback(
				this, hw, sw
			);
		}
	} else if (0x61 == msg[0]) {
		if (0x00 == msg[1]) {
			log("GET: Status Off", XnLogLevel::Info);
			if (m_hist.size() > 0 && dynamic_cast<const XnCmdOff*>(&m_hist.front().cmd) != nullptr)
				hist_ok();
		} else if (0x01 == msg[1]) {
			log("GET: Status On", XnLogLevel::Info);
			if (m_hist.size() > 0 && dynamic_cast<const XnCmdOn*>(&m_hist.front().cmd) != nullptr)
				hist_ok();
		} else if (0x02 == msg[1]) {
			log("GET: Status Programming", XnLogLevel::Info);
		}
	} else if (0x63 == msg[0]) {
		// command station version
	} else if (0xF2 == msg[0]) {
		if (0x01 == msg[1]) {
			// LI address
			log("GET: LI Address is " + QString(msg[2]), XnLogLevel::Info);
			if (m_hist.size() > 0 &&
			    dynamic_cast<const XnCmdGetLIAddress*>(&m_hist.front().cmd) != nullptr) {

				hist_ok();
				dynamic_cast<const XnCmdGetLIAddress&>(m_hist.front().cmd).callback(
					this, msg[2]
				);
			} else if (m_hist.size() > 0 &&
			           dynamic_cast<const XnCmdSetLIAddress*>(&m_hist.front().cmd) != nullptr) {
				hist_ok();
			}

		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void XpressNet::setTrkStatus(const XnTrkStatus status, CPXnCb ok, CPXnCb err) {
	if (status == XnTrkStatus::Off) {
		send(XnCmdOff(), ok, err);
	} else if (status == XnTrkStatus::On) {
		send(XnCmdOn(), ok, err);
	} else {
		throw EInvalidTrkStatus("This track status cannot be set!");
	}
}

void XpressNet::emergencyStop(const LocoAddr addr, CPXnCb ok, CPXnCb err) {
	send(XnCmdEmergencyStopLoco(addr), ok, err);
}

void XpressNet::emergencyStop(CPXnCb ok, CPXnCb err) {
	send(XnCmdEmergencyStop(), ok, err);
}

void XpressNet::getLIVersion(XnGotLIVersion const callback, CPXnCb err) {
	send(XnCmdGetLIVersion(callback), nullptr, err);
}

void XpressNet::getLIAddress(XnGotLIAddress const callback, CPXnCb err) {
	send(XnCmdGetLIAddress(callback), nullptr, err);
}

void XpressNet::setLIAddress(uint8_t addr, CPXnCb ok, CPXnCb err) {
	send(XnCmdSetLIAddress(addr), ok, err);
}

void XpressNet::PomWriteCv(LocoAddr addr, uint16_t cv, uint8_t value, CPXnCb ok,
                CPXnCb err) {
	send(XnCmdPomWriteCv(addr, cv, value), ok, err);
}

void XpressNet::setSpeed(LocoAddr addr, uint8_t speed, bool direction, CPXnCb ok,
                         CPXnCb err) {
	send(XnCmdSetSpeedDir(addr, speed, direction), ok, err);
}

///////////////////////////////////////////////////////////////////////////////

void XpressNet::hist_ok() {
	if (m_hist.size() < 1) {
		log("History buffer underflow!", XnLogLevel::Warning);
		return;
	}

	XnHistoryItem& hist = m_hist.front();
	m_hist.pop();
	if (nullptr != hist.callback_ok)
		hist.callback_ok->func(this, hist.callback_ok->data);
}

void XpressNet::hist_err() {
	if (m_hist.size() < 1) {
		log("History buffer underflow!", XnLogLevel::Warning);
		return;
	}

	XnHistoryItem& hist = m_hist.front();
	m_hist.pop();

	log("Not responded to command: " + hist.cmd.msg(), XnLogLevel::Error);

	if (nullptr != hist.callback_err)
		hist.callback_err->func(this, hist.callback_err->data);
}

void XpressNet::hist_send() {
	XnHistoryItem& hist = m_hist.front();
	m_hist.pop();

	log("Sending again: " + hist.cmd.msg(), XnLogLevel::Warning);
	send(hist);
}

void XpressNet::m_hist_timer_tick() {
	if (!m_serialPort.isOpen()) {
		while (m_hist.size() > 0)
			hist_err();
	}

	if (m_hist.size() < 1)
		return;

	if (m_hist.front().timeout < QDateTime::currentDateTime())
		hist_send();
}

///////////////////////////////////////////////////////////////////////////////

void XpressNet::log(const QString message, const XnLogLevel loglevel) {
	if (loglevel <= this->loglevel)
		onLog(message, loglevel);
}

///////////////////////////////////////////////////////////////////////////////

template<typename T>
QString XpressNet::dataToStr(T data, size_t len) {
	QString out;
	size_t i = 0;
	for (auto d = data.begin(); (d != data.end() && (len == 0 || i < len)); d++, i++)
		out += QString("%1 ").arg(*d, 2, 16, QLatin1Char('0'));

	return out;
}

///////////////////////////////////////////////////////////////////////////////
