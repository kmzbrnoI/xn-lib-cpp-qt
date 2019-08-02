#include <sstream>

#include "xn.h"

namespace Xn {

XpressNet::XpressNet(QObject *parent) : QObject(parent) {
	m_serialPort.setReadBufferSize(256);

	QObject::connect(&m_serialPort, SIGNAL(readyRead()), this, SLOT(handleReadyRead()));
	QObject::connect(&m_serialPort, SIGNAL(errorOccurred(QSerialPort::SerialPortError)), this,
	                 SLOT(handleError(QSerialPort::SerialPortError)));

	QObject::connect(&m_hist_timer, SIGNAL(timeout()), this, SLOT(m_hist_timer_tick()));
	QObject::connect(&m_serialPort, SIGNAL(aboutToClose()), this, SLOT(sp_about_to_close()));
}

void XpressNet::connect(const QString &portname, uint32_t br, QSerialPort::FlowControl fc) {
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
	onDisconnect();
}

void XpressNet::sp_about_to_close() {
	m_hist_timer.stop();
	while (!m_hist.empty())
		m_hist.pop(); // Should we call error events?
	while (!m_out.empty())
		m_out.pop(); // Should we call error events?
	m_trk_status = XnTrkStatus::Unknown;

	log("Disconnected", XnLogLevel::Info);
}

bool XpressNet::connected() const { return m_serialPort.isOpen(); }
XnTrkStatus XpressNet::getTrkStatus() const { return m_trk_status; }

template <typename Target>
bool XpressNet::is(const XnCmd *x) {
	return (dynamic_cast<const Target *>(x) != nullptr);
}

template <typename Target>
bool XpressNet::is(const XnHistoryItem &h) {
	return (dynamic_cast<const Target *>(h.cmd.get()) != nullptr);
}

void XpressNet::send(MsgType data) {
	uint8_t x = 0;
	for (uint8_t d : data)
		x ^= d;
	data.push_back(x);

	log("PUT: " + dataToStr(data), XnLogLevel::RawData);
	QByteArray qdata(reinterpret_cast<const char *>(data.data()), data.size());

	int sent = m_serialPort.write(qdata);

	if (sent == -1 || sent != qdata.size())
		throw EWriteError("No data could we written!");
}

void XpressNet::to_send(std::unique_ptr<const XnCmd> &cmd, UPXnCb ok, UPXnCb err) {
	// Sends or queues
	if (m_hist.size() >= _MAX_HIST_BUF_COUNT) {
		m_out.push(XnHistoryItem(cmd, timeout(cmd.get()), 1, std::move(ok), std::move(err)));
	} else {
		send(std::move(cmd), std::move(ok), std::move(err));
	}
}

void XpressNet::send(std::unique_ptr<const XnCmd> cmd, UPXnCb ok, UPXnCb err) {
	log("PUT: " + cmd->msg(), XnLogLevel::Commands);

	try {
		send(cmd->getBytes());
		m_hist.push(XnHistoryItem(cmd, timeout(cmd.get()), 1, std::move(ok), std::move(err)));
	} catch (QStrException &e) {
		log("Fatal error when writing command: " + cmd->msg(), XnLogLevel::Error);
		if (nullptr != err)
			err->func(this, err->data);
	}
}

template <typename T>
void XpressNet::to_send(const T &&cmd, UPXnCb ok, UPXnCb err) {
	std::unique_ptr<const XnCmd> cmd2(std::make_unique<const T>(cmd));
	to_send(cmd2, std::move(ok), std::move(err));
}

void XpressNet::send(XnHistoryItem &&hist) {
	hist.no_sent++;
	hist.timeout = timeout(hist.cmd.get());

	log("PUT: " + hist.cmd->msg(), XnLogLevel::Commands);

	try {
		send(hist.cmd->getBytes());
		m_hist.push(std::move(hist));
	} catch (QStrException &e) {
		log("PUT ERR: " + e, XnLogLevel::Error);
		throw;
	}
}

QDateTime XpressNet::timeout(const XnCmd *x) {
	QDateTime timeout;
	if (is<XnCmdReadDirect>(x) || is<XnCmdRequestReadResult>(x))
		timeout = QDateTime::currentDateTime().addMSecs(_HIST_PROG_TIMEOUT);
	else
		timeout = QDateTime::currentDateTime().addMSecs(_HIST_TIMEOUT);
	return timeout;
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

		log("GET: " + dataToStr(m_readData, length), XnLogLevel::RawData);

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

void XpressNet::parseMessage(MsgType &msg) {
	switch (static_cast<XnRecvCmdType>(msg[0])) {
	case XnRecvCmdType::LiError:
		return handleMsgLiError(msg);
	case XnRecvCmdType::LiVersion:
		return handleMsgLiVersion(msg);
	case XnRecvCmdType::LiSettings:
		if (0x01 == msg[1])
			return handleMsgLIAddr(msg);
		else
			return;
	case XnRecvCmdType::CsGeneralEvent:
		return handleMsgCsGeneralEvent(msg);
	case XnRecvCmdType::CsStatus:
		if (0x22 == msg[1])
			return handleMsgCsStatus(msg);
		else
			return;
	case XnRecvCmdType::CsX63:
		if (0x21 == msg[1])
			return handleMsgCsVersion(msg);
		else if (0x14 == msg[1])
			return handleMsgCvRead(msg);
		else
			return;
	case XnRecvCmdType::CsLocoInfo:
		return handleMsgLocoInfo(msg);
	case XnRecvCmdType::CsAccInfoResp:
		return handleMsgAcc(msg);
	}
}

void XpressNet::handleMsgLiError(MsgType &msg) {
	if (0x01 == msg[1]) {
		log("GET: Error occurred between the interfaces and the PC", XnLogLevel::Error);
	} else if (0x02 == msg[1]) {
		log("GET: Error occurred between the interfaces and the command station",
		    XnLogLevel::Error);
	} else if (0x03 == msg[1]) {
		log("GET: Unknown communication error", XnLogLevel::Error);
	} else if (0x04 == msg[1]) {
		log("GET: OK", XnLogLevel::Commands);

		if (!m_hist.empty() && is<XnCmdReadDirect>(m_hist.front())) {
			const XnCmdReadDirect &rd =
			    dynamic_cast<const XnCmdReadDirect &>(*(m_hist.front().cmd));
			to_send(XnCmdRequestReadResult(rd.cv, rd.callback), nullptr,
			        std::move(m_hist.front().callback_err));
		}
		hist_ok();
	} else if (0x05 == msg[1]) {
		log("GET: GET: The Command Station is no longer providing the LI "
		    "a timeslot for communication",
		    XnLogLevel::Error);
		hist_err();
	} else if (0x06 == msg[1]) {
		log("GET: GET: Buffer overflow in the LI", XnLogLevel::Error);
	}
}

void XpressNet::handleMsgLiVersion(MsgType &msg) {
	unsigned hw = (msg[1] & 0x0F) + 10*(msg[1] >> 4);
	unsigned sw = (msg[2] & 0x0F) + 10*(msg[2] >> 4);

	log("GET: LI version; HW: " + QString::number(hw) + ", SW: " + QString::number(sw),
	    XnLogLevel::Commands);

	if (is<XnCmdGetLIVersion>(m_hist.front())) {
		std::unique_ptr<const XnCmd> cmd = std::move(m_hist.front().cmd);
		hist_ok();
		const XnCmdGetLIVersion &hist = dynamic_cast<const XnCmdGetLIVersion&>(*cmd.get());
		if (hist.callback != nullptr)
			hist.callback(this, hw, sw);
	}
}

void XpressNet::handleMsgCsGeneralEvent(MsgType &msg) {
	if (0x00 == msg[1]) {
		log("GET: Status Off", XnLogLevel::Commands);
		if (!m_hist.empty() > 0 && is<XnCmdOff>(m_hist.front()))
			hist_ok();
		if (m_trk_status != XnTrkStatus::Off) {
			m_trk_status = XnTrkStatus::Off;
			onTrkStatusChanged(m_trk_status);
		}
	} else if (0x01 == msg[1]) {
		log("GET: Status On", XnLogLevel::Commands);
		if (!m_hist.empty() > 0 && is<XnCmdOn>(m_hist.front()))
			hist_ok();
		if (m_trk_status != XnTrkStatus::On) {
			m_trk_status = XnTrkStatus::On;
			onTrkStatusChanged(m_trk_status);
		}
	} else if (0x02 == msg[1]) {
		log("GET: Status Programming", XnLogLevel::Commands);
		if (m_trk_status != XnTrkStatus::Programming) {
			m_trk_status = XnTrkStatus::Programming;
			onTrkStatusChanged(m_trk_status);
		}
	} else if (0x11 == msg[1] || 0x12 == msg[1] || 0x13 == msg[1] || 0x1F == msg[1]) {
		log("GET: CV read error " + QString::number(msg[1]), XnLogLevel::Error);
		if (!m_hist.empty() && is<XnCmdRequestReadResult>(m_hist.front())) {
			std::unique_ptr<const XnCmd> cmd = std::move(m_hist.front().cmd);
			hist_ok();
			dynamic_cast<const XnCmdRequestReadResult *>(cmd.get())->callback(
			    this, static_cast<XnReadCVStatus>(msg[1]),
			    dynamic_cast<const XnCmdRequestReadResult *>(cmd.get())->cv, 0);
		}
	} else if (0x80 == msg[1]) {
		log("GET: command station reported transfer errors", XnLogLevel::Error);
	} else if (0x81 == msg[1]) {
		log("GET: command station busy", XnLogLevel::Error);
	} else if (0x82 == msg[1]) {
		log("GET: instruction not supported by command station", XnLogLevel::Error);
	}
}

void XpressNet::handleMsgCsStatus(MsgType &msg) {
	log("GET: command station status", XnLogLevel::Commands);
	XnTrkStatus n;
	if (msg[2] & 0x03)
		n = XnTrkStatus::Off;
	else if ((msg[2] >> 3) & 0x01)
		n = XnTrkStatus::Programming;
	else
		n = XnTrkStatus::On;

	if (!m_hist.empty() && is<XnCmdGetCSStatus>(m_hist.front()))
		hist_ok();

	if (n != m_trk_status) {
		m_trk_status = n;
		onTrkStatusChanged(m_trk_status);
	}
}

void XpressNet::handleMsgCsVersion(MsgType &msg) {
	if (!m_hist.empty() > 0 && is<XnCmdGetCSVersion>(m_hist.front())) {
		std::unique_ptr<const XnCmd> cmd = std::move(m_hist.front().cmd);
		hist_ok();
		if (dynamic_cast<const XnCmdGetCSVersion *>(cmd.get())->callback != nullptr) {
			dynamic_cast<const XnCmdGetCSVersion *>(cmd.get())->callback(
				this, msg[2] >> 4, msg[2] & 0x0F
			);
		}
	}
}

void XpressNet::handleMsgCvRead(MsgType &msg) {
	log("GET: CV " + QString::number(msg[2]) + " value=" + QString::number(msg[3]),
	    XnLogLevel::Commands);
	if (!m_hist.empty() > 0 && is<XnCmdRequestReadResult>(m_hist.front())) {
		std::unique_ptr<const XnCmd> cmd = std::move(m_hist.front().cmd);
		hist_ok();
		dynamic_cast<const XnCmdRequestReadResult *>(cmd.get())->callback(
			this, XnReadCVStatus::Ok, msg[2], msg[3]
		);
	}
}

void XpressNet::handleMsgLocoInfo(MsgType &msg) {
	log("GET: loco information", XnLogLevel::Commands);

	if (!m_hist.empty() > 0 && is<XnCmdGetLocoInfo>(m_hist.front())) {
		std::unique_ptr<const XnCmd> cmd = std::move(m_hist.front().cmd);
		hist_ok();

		bool used = (msg[1] >> 3) & 0x01;
		unsigned mode = msg[1] & 0x07;
		auto direction = static_cast<XnDirection>(msg[2] >> 7);
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
			// 28 speed steps
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

		if (dynamic_cast<const XnCmdGetLocoInfo *>(cmd.get())->callback != nullptr) {
			dynamic_cast<const XnCmdGetLocoInfo *>(cmd.get())->callback(
			    this, used, direction, speed, XnFA(msg[3]), XnFB(msg[4]));
		}
	}
}

void XpressNet::handleMsgLIAddr(MsgType &msg) {
	log("GET: LI Address is " + QString::number(msg[2]), XnLogLevel::Commands);
	if (!m_hist.empty() > 0 && is<XnCmdGetLIAddress>(m_hist.front())) {
		std::unique_ptr<const XnCmd> cmd = std::move(m_hist.front().cmd);
		hist_ok();
		if (dynamic_cast<const XnCmdGetLIAddress *>(cmd.get())->callback != nullptr) {
			dynamic_cast<const XnCmdGetLIAddress *>(cmd.get())->callback(this, msg[2]);
		}
	} else if (!m_hist.empty() > 0 && is<XnCmdSetLIAddress>(m_hist.front())) {
		hist_ok();
	}
}

void XpressNet::handleMsgAcc(MsgType &msg) {
	uint8_t groupAddr = msg[1];
	bool nibble = (msg[2] >> 4) & 0x1;
	bool error = msg[2] >> 7;
	XnFeedbackType inputType = static_cast<XnFeedbackType>((msg[2] >> 5) & 0x3);
	XnAccInputsState state;
	state.all = msg[2] & 0x0F;
	log("GET: Acc state: group " + QString::number(groupAddr) + ", nibble " +
	    QString::number(nibble) + ", state " + QString::number(state.all, 2),
	    XnLogLevel::Commands);
	if ((!m_hist.empty() > 0) && (is<XnCmdAccInfoRequest>(m_hist.front())) &&
	    (dynamic_cast<const XnCmdAccInfoRequest *>(m_hist.front().cmd.get())->groupAddr == groupAddr) &&
	    (dynamic_cast<const XnCmdAccInfoRequest *>(m_hist.front().cmd.get())->nibble == nibble))
		hist_ok();
	onAccInputChanged(groupAddr, nibble, error, inputType, state);
}

///////////////////////////////////////////////////////////////////////////////

void XpressNet::setTrkStatus(const XnTrkStatus status, UPXnCb ok, UPXnCb err) {
	if (status == XnTrkStatus::Off) {
		to_send(XnCmdOff(), std::move(ok), std::move(err));
	} else if (status == XnTrkStatus::On) {
		to_send(XnCmdOn(), std::move(ok), std::move(err));
	} else {
		throw EInvalidTrkStatus("This track status cannot be set!");
	}
}

void XpressNet::emergencyStop(const LocoAddr addr, UPXnCb ok, UPXnCb err) {
	to_send(XnCmdEmergencyStopLoco(addr), std::move(ok), std::move(err));
}

void XpressNet::emergencyStop(UPXnCb ok, UPXnCb err) {
	to_send(XnCmdEmergencyStop(), std::move(ok), std::move(err));
}

void XpressNet::getCommandStationVersion(XnGotCSVersion const &callback, UPXnCb err) {
	to_send(XnCmdGetCSVersion(callback), nullptr, std::move(err));
}

void XpressNet::getCommandStationStatus(UPXnCb ok, UPXnCb err) {
	to_send(XnCmdGetCSStatus(), std::move(ok), std::move(err));
}

void XpressNet::getLIVersion(XnGotLIVersion const &callback, UPXnCb err) {
	to_send(XnCmdGetLIVersion(callback), nullptr, std::move(err));
}

void XpressNet::getLIAddress(XnGotLIAddress const &callback, UPXnCb err) {
	to_send(XnCmdGetLIAddress(callback), nullptr, std::move(err));
}

void XpressNet::setLIAddress(uint8_t addr, UPXnCb ok, UPXnCb err) {
	to_send(XnCmdSetLIAddress(addr), std::move(ok), std::move(err));
}

void XpressNet::pomWriteCv(const LocoAddr addr, uint16_t cv, uint8_t value, UPXnCb ok, UPXnCb err) {
	to_send(XnCmdPomWriteCv(addr, cv, value), std::move(ok), std::move(err));
}

void XpressNet::pomWriteBit(const LocoAddr addr, uint16_t cv, uint8_t biti, bool value, UPXnCb ok,
                            UPXnCb err) {
	to_send(XnCmdPomWriteBit(addr, cv, biti, value), std::move(ok), std::move(err));
}

void XpressNet::setSpeed(const LocoAddr addr, uint8_t speed, XnDirection direction, UPXnCb ok,
                         UPXnCb err) {
	to_send(XnCmdSetSpeedDir(addr, speed, direction), std::move(ok), std::move(err));
}

void XpressNet::getLocoInfo(const LocoAddr addr, XnGotLocoInfo const &callback, UPXnCb err) {
	to_send(XnCmdGetLocoInfo(addr, callback), nullptr, std::move(err));
}

void XpressNet::setFuncA(const LocoAddr addr, const XnFA fa, UPXnCb ok, UPXnCb err) {
	to_send(XnCmdSetFuncA(addr, fa), std::move(ok), std::move(err));
}

void XpressNet::setFuncB(const LocoAddr addr, const XnFB fb, const XnFSet range, UPXnCb ok,
                         UPXnCb err) {
	to_send(XnCmdSetFuncB(addr, fb, range), std::move(ok), std::move(err));
}

void XpressNet::readCVdirect(const uint8_t cv, XnReadCV const &callback, UPXnCb err) {
	to_send(XnCmdReadDirect(cv, callback), nullptr, std::move(err));
}

void XpressNet::accInfoRequest(const uint8_t groupAddr, const bool nibble, UPXnCb err) {
	to_send(XnCmdAccInfoRequest(groupAddr, nibble), nullptr, std::move(err));
}

void XpressNet::accOpRequest(const uint16_t portAddr, const bool state, UPXnCb ok, UPXnCb err) {
	to_send(XnCmdAccOpRequest(portAddr, state), std::move(ok), std::move(err));
}

///////////////////////////////////////////////////////////////////////////////

void XpressNet::hist_ok() {
	if (m_hist.empty()) {
		log("History buffer underflow!", XnLogLevel::Warning);
		return;
	}

	XnHistoryItem hist = std::move(m_hist.front());
	m_hist.pop();
	if (nullptr != hist.callback_ok)
		hist.callback_ok->func(this, hist.callback_ok->data);
	if (m_out.size() > 0)
		send_next_out();
}

void XpressNet::hist_err() {
	if (m_hist.empty()) {
		log("History buffer underflow!", XnLogLevel::Warning);
		return;
	}

	XnHistoryItem hist = std::move(m_hist.front());
	m_hist.pop();

	log("Not responded to command: " + hist.cmd->msg(), XnLogLevel::Error);

	if (nullptr != hist.callback_err)
		hist.callback_err->func(this, hist.callback_err->data);
	if (m_out.size() > 0)
		send_next_out();
}

void XpressNet::hist_send() {
	XnHistoryItem hist = std::move(m_hist.front());
	m_hist.pop();

	log("Sending again: " + hist.cmd->msg(), XnLogLevel::Warning);

	try {
		send(std::move(hist));
	} catch (...) {}
}

void XpressNet::m_hist_timer_tick() {
	if (!m_serialPort.isOpen()) {
		while (!m_hist.empty())
			hist_err();
	}

	if (m_hist.empty())
		return;

	if (m_hist.front().timeout < QDateTime::currentDateTime()) {
		if (m_hist.front().no_sent >= _HIST_SEND_MAX)
			hist_err();
		else
			hist_send();
	}
}

void XpressNet::send_next_out() {
	XnHistoryItem out = std::move(m_out.front());
	m_out.pop();
	out.no_sent = 0;
	send(std::move(out));
}

///////////////////////////////////////////////////////////////////////////////

void XpressNet::log(const QString &message, const XnLogLevel loglevel) {
	if (loglevel <= this->loglevel)
		onLog(message, loglevel);
}

///////////////////////////////////////////////////////////////////////////////

template <typename DataT>
QString XpressNet::dataToStr(DataT data, size_t len) {
	QString out;
	size_t i = 0;
	for (auto d = data.begin(); (d != data.end() && (len == 0 || i < len)); d++, i++)
		out += QString("0x%1 ").arg(*d, 2, 16, QLatin1Char('0'));

	return out;
}

///////////////////////////////////////////////////////////////////////////////

QString XpressNet::xnReadCVStatusToQString(const XnReadCVStatus st) {
	if (st == XnReadCVStatus::Ok)
		return "Ok";
	if (st == XnReadCVStatus::ShortCircuit)
		return "Short Circuit";
	if (st == XnReadCVStatus::DataByteNotFound)
		return "Data Byte Not Found";
	if (st == XnReadCVStatus::CSbusy)
		return "Command station busy";
	if (st == XnReadCVStatus::CSready)
		return "Command station ready";

	return "Unknown error";
}

} // namespace Xn
