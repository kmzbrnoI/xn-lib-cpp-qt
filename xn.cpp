#include <sstream>

#include "xn.h"

namespace Xn {

XpressNet::XpressNet(QObject *parent) : QObject(parent) {
	m_serialPort.setReadBufferSize(256);

	QObject::connect(&m_serialPort, SIGNAL(readyRead()), this, SLOT(handleReadyRead()));
	QObject::connect(&m_serialPort, SIGNAL(errorOccurred(QSerialPort::SerialPortError)),
	                 this, SLOT(handleError(QSerialPort::SerialPortError)));

	QObject::connect(&m_hist_timer, SIGNAL(timeout()), this, SLOT(m_hist_timer_tick()));
	QObject::connect(&m_serialPort, SIGNAL(aboutToClose()), this, SLOT(sp_about_to_close()));
}

void XpressNet::connect(QString portname, uint32_t br, QSerialPort::FlowControl fc) {
	log("Connecting to " + portname + " (br=" + QString::number(br) + ") ...", XnLogLevel::Info);

	m_serialPort.setBaudRate(br);
	m_serialPort.setFlowControl(fc);
	m_serialPort.setPortName(portname);

	if (!m_serialPort.open(QIODevice::ReadWrite))
		throw EOpenError(m_serialPort.errorString());

	m_hist_timer.start(_HIST_CHECK_INTERVAL);
	log("Connected", XnLogLevel::Info);
	onConnect();
}

void XpressNet::disconnect() {
	log("Disconnecting...", XnLogLevel::Info);
	m_hist_timer.stop();
	m_serialPort.close();
}

void XpressNet::sp_about_to_close() {
	m_hist_timer.stop();
	while (m_hist.size() > 0)
		m_hist.pop(); // Should we call error events?
	m_trk_status = XnTrkStatus::Unknown;

	log("Disconnected", XnLogLevel::Info);
	onDisconnect();
}

bool XpressNet::connected() const {
	return m_serialPort.isOpen();
}

XnTrkStatus XpressNet::getTrkStatus() const {
	return m_trk_status;
}

void XpressNet::send(const std::vector<uint8_t> data) {
	QByteArray qdata(reinterpret_cast<const char*>(data.data()), data.size());

	uint8_t x = 0;
	for (uint8_t d : data)
		x ^= d;
	qdata.append(x);

	log("PUT: " + dataToStr(qdata), XnLogLevel::Data);

	int sent = m_serialPort.write(qdata);

	if (sent == -1 || sent != qdata.size()) {
		throw EWriteError("No data could we written!");
	}
}

void XpressNet::send(std::unique_ptr<const XnCmd>& cmd, UPXnCb ok, UPXnCb err) {
	log("PUT: " + cmd->msg(), XnLogLevel::Info);

	try {
		send(cmd->getBytes());
		m_hist.push(XnHistoryItem(
			cmd, QDateTime::currentDateTime().addMSecs(_HIST_TIMEOUT), 1,
			std::move(ok), std::move(err)
		));
	}
	catch (QStrException& e) {
		log("PUT ERR: " + e, XnLogLevel::Error);
		throw;
	}
}

template<typename T>
void XpressNet::send(const T&& cmd, UPXnCb ok, UPXnCb err) {
	std::unique_ptr<const XnCmd> cmd2(std::make_unique<const T>(cmd));
	send(cmd2, std::move(ok), std::move(err));
}

void XpressNet::send(XnHistoryItem&& hist) {
	hist.no_sent++;
	hist.timeout = QDateTime::currentDateTime().addMSecs(_HIST_TIMEOUT);

	log("PUT: " + hist.cmd->msg(), XnLogLevel::Info);

	try {
		send(hist.cmd->getBytes());
		m_hist.push(std::move(hist));
	}
	catch (QStrException& e) {
		log("PUT ERR: " + e, XnLogLevel::Error);
		throw;
	}
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
	if (serialPortError != QSerialPort::NoError)
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
			hist_err();
		} else if (0x06 == msg[1]) {
			log("GET: GET: Buffer overflow in the LI", XnLogLevel::Error);
		}
	} else if (0x02 == msg[0]) {
		// Got LI version
		unsigned hw = (msg[1] & 0x0F) + 10*(msg[1] >> 4);
		unsigned sw = (msg[2] & 0x0F) + 10*(msg[2] >> 4);

		log("GET: LI version; HW: " + QString::number(hw) + ", SW: " + QString::number(sw), XnLogLevel::Info);

		if (dynamic_cast<const XnCmdGetLIVersion*>(m_hist.front().cmd.get()) != nullptr) {
			if (dynamic_cast<const XnCmdGetLIVersion*>(m_hist.front().cmd.get())->callback != nullptr) {
				dynamic_cast<const XnCmdGetLIVersion*>(m_hist.front().cmd.get())->callback(
					this, hw, sw
				);
			}
			hist_ok();
		}
	} else if (0x61 == msg[0]) {
		if (0x00 == msg[1]) {
			log("GET: Status Off", XnLogLevel::Info);
			if (m_hist.size() > 0 && dynamic_cast<const XnCmdOff*>(m_hist.front().cmd.get()) != nullptr)
				hist_ok();
			m_trk_status = XnTrkStatus::Off;
			onTrkStatusChanged(m_trk_status);
		} else if (0x01 == msg[1]) {
			log("GET: Status On", XnLogLevel::Info);
			if (m_hist.size() > 0 && dynamic_cast<const XnCmdOn*>(m_hist.front().cmd.get()) != nullptr)
				hist_ok();
			m_trk_status = XnTrkStatus::On;
			onTrkStatusChanged(m_trk_status);
		} else if (0x02 == msg[1]) {
			log("GET: Status Programming", XnLogLevel::Info);
			m_trk_status = XnTrkStatus::Programming;
			onTrkStatusChanged(m_trk_status);
		}
	} else if (0x62 == msg[0] && 0x22 == msg[1]) {
		log("GET: command station status", XnLogLevel::Info);
		XnTrkStatus n;
		if (msg[2] & 0x03)
			n = XnTrkStatus::Off;
		else if ((msg[2] >> 3) & 0x01)
			n = XnTrkStatus::Programming;
		else
			n = XnTrkStatus::On;

		if (m_hist.size() > 0 && dynamic_cast<const XnCmdGetCSStatus*>(m_hist.front().cmd.get()) != nullptr)
			hist_ok();

		if (n != m_trk_status) {
			m_trk_status = n;
			onTrkStatusChanged(m_trk_status);
		}
	} else if (0x63 == msg[0] && 0x21 == msg[1]) {
		// command station version
		if (m_hist.size() > 0 &&
			dynamic_cast<const XnCmdGetCSVersion*>(m_hist.front().cmd.get()) != nullptr) {
			if (dynamic_cast<const XnCmdGetCSVersion*>(m_hist.front().cmd.get())->callback != nullptr) {
				dynamic_cast<const XnCmdGetCSVersion*>(m_hist.front().cmd.get())->callback(
					this, msg[2] >> 4, msg[2] & 0x0F
				);
			}
			hist_ok();
		}
	} else if (0xE4 == msg[0]) {
		// Loco information
		log("GET: loco information", XnLogLevel::Info);

		if (m_hist.size() > 0 &&
			dynamic_cast<const XnCmdGetLocoInfo*>(m_hist.front().cmd.get()) != nullptr) {

			bool used = (msg[1] >> 3) & 0x01;
			unsigned mode = msg[1] & 0x07;
			bool direction = msg[2] >> 7;
			unsigned speed;

			// Normalize speed to 28 speed steps
			if (mode == 0) {
				// 14 speed steps
				speed = msg[2] & 0xF;
				if (speed > 0)
					speed -= 1;
				speed *= 2;
			} else if (mode == 1) {
				// 27 speed steps
				speed = ((msg[2] & 0xF) << 1) + ((msg[2] >> 4) & 0x1);
				if (speed < 4)
					speed = 0;
				else
					speed -= 3;
				speed = speed * (28./27);
			} else if (mode == 2) {
				speed = ((msg[2] & 0xF) << 1) + ((msg[2] >> 4) & 0x1);
				if (speed < 4)
					speed = 0;
				else
					speed -= 3;
			} else {
				// 128 speed steps
				speed = msg[2] & 0x7F;
				if (speed > 0)
					speed -= 1;
				speed = speed * (28./128);
			}

			if (dynamic_cast<const XnCmdGetLocoInfo*>(m_hist.front().cmd.get())->callback != nullptr) {
				dynamic_cast<const XnCmdGetLocoInfo*>(m_hist.front().cmd.get())->callback(
					this, used, direction, speed, XnFA(msg[3]), XnFB(msg[4])
				);
			}
			hist_ok();
		}

	} else if (0xF2 == msg[0] && 0x01 == msg[1]) {
		// LI address
		log("GET: LI Address is " + QString::number(msg[2]), XnLogLevel::Info);
		if (m_hist.size() > 0 &&
			dynamic_cast<const XnCmdGetLIAddress*>(m_hist.front().cmd.get()) != nullptr) {
			if (dynamic_cast<const XnCmdGetLIAddress*>(m_hist.front().cmd.get())->callback != nullptr) {
				dynamic_cast<const XnCmdGetLIAddress*>(m_hist.front().cmd.get())->callback(
					this, msg[2]
				);
			}
			hist_ok();
		} else if (m_hist.size() > 0 &&
				   dynamic_cast<const XnCmdSetLIAddress*>(m_hist.front().cmd.get()) != nullptr) {
			hist_ok();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void XpressNet::setTrkStatus(const XnTrkStatus status, UPXnCb ok, UPXnCb err) {
	if (status == XnTrkStatus::Off) {
		send(XnCmdOff(), std::move(ok), std::move(err));
	} else if (status == XnTrkStatus::On) {
		send(XnCmdOn(), std::move(ok), std::move(err));
	} else {
		throw EInvalidTrkStatus("This track status cannot be set!");
	}
}

void XpressNet::emergencyStop(const LocoAddr addr, UPXnCb ok, UPXnCb err) {
	send(XnCmdEmergencyStopLoco(addr), std::move(ok), std::move(err));
}

void XpressNet::emergencyStop(UPXnCb ok, UPXnCb err) {
	send(XnCmdEmergencyStop(), std::move(ok), std::move(err));
}

void XpressNet::getCommandStationVersion(XnGotCSVersion const callback, UPXnCb err) {
	send(XnCmdGetCSVersion(callback), nullptr, std::move(err));
}

void XpressNet::getCommandStationStatus(UPXnCb ok, UPXnCb err) {
	send(XnCmdGetCSStatus(), std::move(ok), std::move(err));
}

void XpressNet::getLIVersion(XnGotLIVersion const callback, UPXnCb err) {
	send(XnCmdGetLIVersion(callback), nullptr, std::move(err));
}

void XpressNet::getLIAddress(XnGotLIAddress const callback, UPXnCb err) {
	send(XnCmdGetLIAddress(callback), nullptr, std::move(err));
}

void XpressNet::setLIAddress(uint8_t addr, UPXnCb ok, UPXnCb err) {
	send(XnCmdSetLIAddress(addr), std::move(ok), std::move(err));
}

void XpressNet::PomWriteCv(LocoAddr addr, uint16_t cv, uint8_t value, UPXnCb ok,
                           UPXnCb err) {
	send(XnCmdPomWriteCv(addr, cv, value), std::move(ok), std::move(err));
}

void XpressNet::setSpeed(LocoAddr addr, uint8_t speed, bool direction, UPXnCb ok,
                         UPXnCb err) {
	send(XnCmdSetSpeedDir(addr, speed, direction), std::move(ok), std::move(err));
}

///////////////////////////////////////////////////////////////////////////////

void XpressNet::hist_ok() {
	if (m_hist.size() < 1) {
		log("History buffer underflow!", XnLogLevel::Warning);
		return;
	}

	XnHistoryItem hist = std::move(m_hist.front());
	m_hist.pop();
	if (nullptr != hist.callback_ok)
		hist.callback_ok->func(this, hist.callback_ok->data);
}

void XpressNet::hist_err() {
	if (m_hist.size() < 1) {
		log("History buffer underflow!", XnLogLevel::Warning);
		return;
	}

	XnHistoryItem hist = std::move(m_hist.front());
	m_hist.pop();

	log("Not responded to command: " + hist.cmd->msg(), XnLogLevel::Error);

	if (nullptr != hist.callback_err)
		hist.callback_err->func(this, hist.callback_err->data);
}

void XpressNet::hist_send() {
	XnHistoryItem hist = std::move(m_hist.front());
	m_hist.pop();

	log("Sending again: " + hist.cmd->msg(), XnLogLevel::Warning);

	try {
		send(std::move(hist));
	}
	catch (...) {}
}

void XpressNet::m_hist_timer_tick() {
	if (!m_serialPort.isOpen()) {
		while (m_hist.size() > 0)
			hist_err();
	}

	if (m_hist.size() < 1)
		return;

	if (m_hist.front().timeout < QDateTime::currentDateTime()) {
		if (m_hist.front().no_sent >= _HIST_SEND_MAX)
			hist_err();
		else
			hist_send();
	}
}

///////////////////////////////////////////////////////////////////////////////

void XpressNet::log(const QString message, const XnLogLevel loglevel) {
	if (loglevel <= this->loglevel)
		onLog(message, loglevel);
}

///////////////////////////////////////////////////////////////////////////////

template<typename DataT>
QString XpressNet::dataToStr(DataT data, size_t len) {
	QString out;
	size_t i = 0;
	for (auto d = data.begin(); (d != data.end() && (len == 0 || i < len)); d++, i++)
		out += QString("0x%1 ").arg(*d, 2, 16, QLatin1Char('0')).rightRef(5);

	return out;
}

///////////////////////////////////////////////////////////////////////////////

}//end namespace
