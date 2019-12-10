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

void XpressNet::connect(const QString &portname, int32_t br, QSerialPort::FlowControl fc,
                        LIType liType) {
	log("Connecting to " + portname + " (br=" + QString::number(br) + ") ...", LogLevel::Info);

	m_serialPort.setBaudRate(br);
	m_serialPort.setFlowControl(fc);
	m_serialPort.setPortName(portname);
	m_liType = liType;

	if (!m_serialPort.open(QIODevice::ReadWrite))
		throw EOpenError(m_serialPort.errorString());

	m_hist_timer.start(_HIST_CHECK_INTERVAL);
	log("Connected", LogLevel::Info);
	onConnect();
}

void XpressNet::disconnect() {
	log("Disconnecting...", LogLevel::Info);
	m_hist_timer.stop();
	m_serialPort.close();
	onDisconnect();
}

void XpressNet::sp_about_to_close() {
	m_hist_timer.stop();
	while (!m_hist.empty())
		m_hist.pop_front(); // Should we call error events?
	while (!m_out.empty())
		m_out.pop(); // Should we call error events?
	m_trk_status = TrkStatus::Unknown;

	log("Disconnected", LogLevel::Info);
}

bool XpressNet::connected() const { return m_serialPort.isOpen(); }
TrkStatus XpressNet::getTrkStatus() const { return m_trk_status; }

template <typename Target>
bool XpressNet::is(const HistoryItem &h) {
	return (dynamic_cast<const Target *>(h.cmd.get()) != nullptr);
}

void XpressNet::send(MsgType data) {
	uint8_t x = 0;
	for (uint8_t d : data)
		x ^= d;
	data.push_back(x);

	log("PUT: " + dataToStr(data), LogLevel::RawData);
	QByteArray qdata(reinterpret_cast<const char *>(data.data()), data.size());

	qint64 sent = m_serialPort.write(qdata);
	if (sent == -1 || sent != qdata.size())
		throw EWriteError("No data could we written!");
}

void XpressNet::to_send(std::unique_ptr<const Cmd> &cmd, UPCb ok, UPCb err) {
	// Sends or queues
	if (m_hist.size() >= _MAX_HIST_BUF_COUNT) {
		m_out.push(HistoryItem(cmd, timeout(cmd.get()), 1, std::move(ok), std::move(err)));
	} else {
		send(std::move(cmd), std::move(ok), std::move(err));
	}
}

void XpressNet::send(std::unique_ptr<const Cmd> cmd, UPCb ok, UPCb err) {
	log("PUT: " + cmd->msg(), LogLevel::Commands);

	try {
		send(cmd->getBytes());
		if (Xn::is<CmdAccOpRequest>(*cmd) && !this->liAcknowledgesSetAccState() &&
			dynamic_cast<const CmdAccOpRequest &>(*cmd).state) {
			// acknowledge manually, do not add to history buffer
			if (nullptr != ok)
				ok->func(this, ok->data);
		} else
			m_hist.emplace_back(cmd, timeout(cmd.get()), 1, std::move(ok), std::move(err));
	} catch (QStrException &) {
		log("Fatal error when writing command: " + cmd->msg(), LogLevel::Error);
		if (nullptr != err)
			err->func(this, err->data);
	}
}

template <typename T>
void XpressNet::to_send(const T &&cmd, UPCb ok, UPCb err) {
	std::unique_ptr<const Cmd> cmd2(std::make_unique<const T>(cmd));
	to_send(cmd2, std::move(ok), std::move(err));
}

void XpressNet::send(HistoryItem &&hist) {
	hist.no_sent++;
	hist.timeout = timeout(hist.cmd.get());

	log("PUT: " + hist.cmd->msg(), LogLevel::Commands);

	try {
		send(hist.cmd->getBytes());
		m_hist.push_back(std::move(hist));
	} catch (QStrException &e) {
		log("PUT ERR: " + e, LogLevel::Error);
		throw;
	}
}

QDateTime XpressNet::timeout(const Cmd *x) {
	QDateTime timeout;
	if (Xn::is<CmdReadDirect>(*x) || Xn::is<CmdRequestReadResult>(*x))
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

		log("GET: " + dataToStr(m_readData, length), LogLevel::RawData);

		if (x != 0) {
			// XOR error
			log("XOR error: " + dataToStr(m_readData, length), LogLevel::Warning);
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
	switch (static_cast<RecvCmdType>(msg[0])) {
	case RecvCmdType::LiError:
		return handleMsgLiError(msg);
	case RecvCmdType::LiVersion:
		return handleMsgLiVersion(msg);
	case RecvCmdType::LiSettings:
		if (0x01 == msg[1])
			return handleMsgLIAddr(msg);
		else
			return;
	case RecvCmdType::CsGeneralEvent:
		return handleMsgCsGeneralEvent(msg);
	case RecvCmdType::CsStatus:
		if (0x22 == msg[1])
			return handleMsgCsStatus(msg);
		else
			return;
	case RecvCmdType::CsX63:
		if (0x21 == msg[1])
			return handleMsgCsVersion(msg);
		else if (0x14 == msg[1])
			return handleMsgCvRead(msg);
		else
			return;
	case RecvCmdType::CsLocoInfo:
		return handleMsgLocoInfo(msg);
	case RecvCmdType::CsAccInfoResp:
		return handleMsgAcc(msg);
	}
}

void XpressNet::handleMsgLiError(MsgType &msg) {
	if (0x01 == msg[1]) {
		log("GET: Error occurred between the interfaces and the PC", LogLevel::Error);
	} else if (0x02 == msg[1]) {
		log("GET: Error occurred between the interfaces and the command station",
		    LogLevel::Error);
	} else if (0x03 == msg[1]) {
		log("GET: Unknown communication error", LogLevel::Error);
	} else if (0x04 == msg[1]) {
		log("GET: OK", LogLevel::Commands);

		if (!m_hist.empty() && is<CmdReadDirect>(m_hist.front())) {
			const auto &rd = dynamic_cast<const CmdReadDirect &>(*(m_hist.front().cmd));
			to_send(CmdRequestReadResult(rd.cv, rd.callback), nullptr,
			        std::move(m_hist.front().callback_err));
		}
		hist_ok();
	} else if (0x05 == msg[1]) {
		log("GET: GET: The Command Station is no longer providing the LI "
		    "a timeslot for communication",
		    LogLevel::Error);
		hist_err();
	} else if (0x06 == msg[1]) {
		log("GET: GET: Buffer overflow in the LI", LogLevel::Error);
	}
}

void XpressNet::handleMsgLiVersion(MsgType &msg) {
	unsigned hw = (msg[1] & 0x0F) + 10*(msg[1] >> 4);
	unsigned sw = (msg[2] & 0x0F) + 10*(msg[2] >> 4);

	log("GET: LI version; HW: " + QString::number(hw) + ", SW: " + QString::number(sw),
	    LogLevel::Commands);

	if (!m_hist.empty() && is<CmdGetLIVersion>(m_hist.front())) {
		std::unique_ptr<const Cmd> cmd = std::move(m_hist.front().cmd);
		hist_ok();
		const auto &hist = dynamic_cast<const CmdGetLIVersion&>(*cmd);
		if (hist.callback != nullptr)
			hist.callback(this, hw, sw);
	}
}

void XpressNet::handleMsgCsGeneralEvent(MsgType &msg) {
	if (0x00 == msg[1]) {
		log("GET: Status Off", LogLevel::Commands);
		if (!m_hist.empty() > 0 && is<CmdOff>(m_hist.front()))
			hist_ok();
		if (m_trk_status != TrkStatus::Off) {
			m_trk_status = TrkStatus::Off;
			onTrkStatusChanged(m_trk_status);
		}
	} else if (0x01 == msg[1]) {
		log("GET: Status On", LogLevel::Commands);
		if (!m_hist.empty() > 0 && is<CmdOn>(m_hist.front()))
			hist_ok();
		if (m_trk_status != TrkStatus::On) {
			m_trk_status = TrkStatus::On;
			onTrkStatusChanged(m_trk_status);
		}
	} else if (0x02 == msg[1]) {
		log("GET: Status Programming", LogLevel::Commands);
		if (m_trk_status != TrkStatus::Programming) {
			m_trk_status = TrkStatus::Programming;
			onTrkStatusChanged(m_trk_status);
		}
	} else if (0x11 == msg[1] || 0x12 == msg[1] || 0x13 == msg[1] || 0x1F == msg[1]) {
		log("GET: CV read error " + QString::number(msg[1]), LogLevel::Error);
		if (!m_hist.empty() && is<CmdRequestReadResult>(m_hist.front())) {
			std::unique_ptr<const Cmd> cmd = std::move(m_hist.front().cmd);
			hist_ok();
			dynamic_cast<const CmdRequestReadResult *>(cmd.get())->callback(
			    this, static_cast<ReadCVStatus>(msg[1]),
			    dynamic_cast<const CmdRequestReadResult *>(cmd.get())->cv, 0);
		}
	} else if (0x80 == msg[1]) {
		log("GET: command station reported transfer errors", LogLevel::Error);
	} else if (0x81 == msg[1]) {
		log("GET: command station busy", LogLevel::Error);
	} else if (0x82 == msg[1]) {
		log("GET: instruction not supported by command station", LogLevel::Error);
	}
}

void XpressNet::handleMsgCsStatus(MsgType &msg) {
	log("GET: command station status", LogLevel::Commands);
	TrkStatus n;
	if (msg[2] & 0x03)
		n = TrkStatus::Off;
	else if ((msg[2] >> 3) & 0x01)
		n = TrkStatus::Programming;
	else
		n = TrkStatus::On;

	if (!m_hist.empty() && is<CmdGetCSStatus>(m_hist.front()))
		hist_ok();

	if (n != m_trk_status) {
		m_trk_status = n;
		onTrkStatusChanged(m_trk_status);
	}
}

void XpressNet::handleMsgCsVersion(MsgType &msg) {
	if (!m_hist.empty() > 0 && is<CmdGetCSVersion>(m_hist.front())) {
		std::unique_ptr<const Cmd> cmd = std::move(m_hist.front().cmd);
		hist_ok();
		if (dynamic_cast<const CmdGetCSVersion *>(cmd.get())->callback != nullptr) {
			dynamic_cast<const CmdGetCSVersion *>(cmd.get())->callback(
				this, msg[2] >> 4, msg[2] & 0x0F
			);
		}
	}
}

void XpressNet::handleMsgCvRead(MsgType &msg) {
	log("GET: CV " + QString::number(msg[2]) + " value=" + QString::number(msg[3]),
	    LogLevel::Commands);
	if (!m_hist.empty() > 0 && is<CmdRequestReadResult>(m_hist.front())) {
		std::unique_ptr<const Cmd> cmd = std::move(m_hist.front().cmd);
		hist_ok();
		dynamic_cast<const CmdRequestReadResult *>(cmd.get())->callback(
			this, ReadCVStatus::Ok, msg[2], msg[3]
		);
	}
}

void XpressNet::handleMsgLocoInfo(MsgType &msg) {
	log("GET: loco information", LogLevel::Commands);

	if (!m_hist.empty() > 0 && is<CmdGetLocoInfo>(m_hist.front())) {
		std::unique_ptr<const Cmd> cmd = std::move(m_hist.front().cmd);
		hist_ok();

		bool used = (msg[1] >> 3) & 0x01;
		unsigned mode = msg[1] & 0x07;
		auto direction = static_cast<Direction>(msg[2] >> 7);
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

		if (dynamic_cast<const CmdGetLocoInfo *>(cmd.get())->callback != nullptr) {
			dynamic_cast<const CmdGetLocoInfo *>(cmd.get())->callback(
			    this, used, direction, speed, FA(msg[3]), FB(msg[4]));
		}
	}
}

void XpressNet::handleMsgLIAddr(MsgType &msg) {
	log("GET: LI Address is " + QString::number(msg[2]), LogLevel::Commands);
	if (!m_hist.empty() > 0 && is<CmdGetLIAddress>(m_hist.front())) {
		std::unique_ptr<const Cmd> cmd = std::move(m_hist.front().cmd);
		hist_ok();
		if (dynamic_cast<const CmdGetLIAddress *>(cmd.get())->callback != nullptr) {
			dynamic_cast<const CmdGetLIAddress *>(cmd.get())->callback(this, msg[2]);
		}
	} else if (!m_hist.empty() > 0 && is<CmdSetLIAddress>(m_hist.front())) {
		hist_ok();
	}
}

void XpressNet::handleMsgAcc(MsgType &msg) {
	uint8_t groupAddr = msg[1];
	bool nibble = (msg[2] >> 4) & 0x1;
	bool error = msg[2] >> 7;
	auto inputType = static_cast<FeedbackType>((msg[2] >> 5) & 0x3);
	AccInputsState state;
	state.all = msg[2] & 0x0F;
	log("GET: Acc state: group " + QString::number(groupAddr) + ", nibble " +
		QString::number(nibble) + ", state " + QString::number(state.all, 2).rightJustified(4, '0'),
	    LogLevel::Commands);
	if ((!m_hist.empty() > 0) && (is<CmdAccInfoRequest>(m_hist.front())) &&
	    (dynamic_cast<const CmdAccInfoRequest *>(m_hist.front().cmd.get())->groupAddr == groupAddr) &&
	    (dynamic_cast<const CmdAccInfoRequest *>(m_hist.front().cmd.get())->nibble == nibble))
		hist_ok();
	onAccInputChanged(groupAddr, nibble, error, inputType, state);
}

///////////////////////////////////////////////////////////////////////////////

void XpressNet::setTrkStatus(const TrkStatus status, UPCb ok, UPCb err) {
	if (status == TrkStatus::Off) {
		to_send(CmdOff(), std::move(ok), std::move(err));
	} else if (status == TrkStatus::On) {
		to_send(CmdOn(), std::move(ok), std::move(err));
	} else {
		throw EInvalidTrkStatus("This track status cannot be set!");
	}
}

void XpressNet::emergencyStop(const LocoAddr addr, UPCb ok, UPCb err) {
	to_send(CmdEmergencyStopLoco(addr), std::move(ok), std::move(err));
}

void XpressNet::emergencyStop(UPCb ok, UPCb err) {
	to_send(CmdEmergencyStop(), std::move(ok), std::move(err));
}

void XpressNet::getCommandStationVersion(GotCSVersion const &callback, UPCb err) {
	to_send(CmdGetCSVersion(callback), nullptr, std::move(err));
}

void XpressNet::getCommandStationStatus(UPCb ok, UPCb err) {
	to_send(CmdGetCSStatus(), std::move(ok), std::move(err));
}

void XpressNet::getLIVersion(GotLIVersion const &callback, UPCb err) {
	to_send(CmdGetLIVersion(callback), nullptr, std::move(err));
}

void XpressNet::getLIAddress(GotLIAddress const &callback, UPCb err) {
	to_send(CmdGetLIAddress(callback), nullptr, std::move(err));
}

void XpressNet::setLIAddress(uint8_t addr, UPCb ok, UPCb err) {
	to_send(CmdSetLIAddress(addr), std::move(ok), std::move(err));
}

void XpressNet::pomWriteCv(const LocoAddr addr, uint16_t cv, uint8_t value, UPCb ok, UPCb err) {
	to_send(CmdPomWriteCv(addr, cv, value), std::move(ok), std::move(err));
}

void XpressNet::pomWriteBit(const LocoAddr addr, uint16_t cv, uint8_t biti, bool value, UPCb ok,
                            UPCb err) {
	to_send(CmdPomWriteBit(addr, cv, biti, value), std::move(ok), std::move(err));
}

void XpressNet::setSpeed(const LocoAddr addr, uint8_t speed, Direction direction, UPCb ok,
                         UPCb err) {
	to_send(CmdSetSpeedDir(addr, speed, direction), std::move(ok), std::move(err));
}

void XpressNet::getLocoInfo(const LocoAddr addr, GotLocoInfo const &callback, UPCb err) {
	to_send(CmdGetLocoInfo(addr, callback), nullptr, std::move(err));
}

void XpressNet::setFuncA(const LocoAddr addr, const FA fa, UPCb ok, UPCb err) {
	to_send(CmdSetFuncA(addr, fa), std::move(ok), std::move(err));
}

void XpressNet::setFuncB(const LocoAddr addr, const FB fb, const FSet range, UPCb ok,
                         UPCb err) {
	to_send(CmdSetFuncB(addr, fb, range), std::move(ok), std::move(err));
}

void XpressNet::readCVdirect(const uint8_t cv, ReadCV const &callback, UPCb err) {
	to_send(CmdReadDirect(cv, callback), nullptr, std::move(err));
}

void XpressNet::accInfoRequest(const uint8_t groupAddr, const bool nibble, UPCb err) {
	to_send(CmdAccInfoRequest(groupAddr, nibble), nullptr, std::move(err));
}

void XpressNet::accOpRequest(const uint16_t portAddr, const bool state, UPCb ok, UPCb err) {
	to_send(CmdAccOpRequest(portAddr, state), std::move(ok), std::move(err));
}

///////////////////////////////////////////////////////////////////////////////

void XpressNet::hist_ok() {
	if (m_hist.empty()) {
		log("History buffer underflow!", LogLevel::Warning);
		return;
	}

	HistoryItem hist = std::move(m_hist.front());
	m_hist.pop_front();
	if (nullptr != hist.callback_ok)
		hist.callback_ok->func(this, hist.callback_ok->data);
	if (!m_out.empty())
		send_next_out();
}

void XpressNet::hist_err() {
	if (m_hist.empty()) {
		log("History buffer underflow!", LogLevel::Warning);
		return;
	}

	HistoryItem hist = std::move(m_hist.front());
	m_hist.pop_front();

	log("Not responded to command: " + hist.cmd->msg(), LogLevel::Error);

	if (nullptr != hist.callback_err)
		hist.callback_err->func(this, hist.callback_err->data);
	if (!m_out.empty())
		send_next_out();
}

void XpressNet::hist_send() {
	HistoryItem hist = std::move(m_hist.front());
	m_hist.pop_front();

	for(const HistoryItem &newer : m_hist) {
		if (hist.cmd->conflict(*(newer.cmd)) || newer.cmd->conflict(*(hist.cmd))) {
			log("Not sending again, conflict: " + hist.cmd->msg() + ", " + newer.cmd->msg(),
			    LogLevel::Warning);
			if (nullptr != hist.callback_err)
				hist.callback_err->func(this, hist.callback_err->data);
			return;
		}
	}

	log("Sending again: " + hist.cmd->msg(), LogLevel::Warning);

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
	HistoryItem out = std::move(m_out.front());
	m_out.pop();
	out.no_sent = 0;
	send(std::move(out));
}

void XpressNet::histClear() {
	size_t hist_size = m_hist.size();
	for (size_t i = 0; i < hist_size; ++i)
		hist_err(); // can add next items to history!
}

///////////////////////////////////////////////////////////////////////////////

void XpressNet::log(const QString &message, const LogLevel loglevel) {
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

QString XpressNet::xnReadCVStatusToQString(const ReadCVStatus st) {
	if (st == ReadCVStatus::Ok)
		return "Ok";
	if (st == ReadCVStatus::ShortCircuit)
		return "Short Circuit";
	if (st == ReadCVStatus::DataByteNotFound)
		return "Data Byte Not Found";
	if (st == ReadCVStatus::CSbusy)
		return "Command station busy";
	if (st == ReadCVStatus::CSready)
		return "Command station ready";

	return "Unknown error";
}

///////////////////////////////////////////////////////////////////////////////

bool XpressNet::liAcknowledgesSetAccState() const { return (this->m_liType == LIType::uLI); }

///////////////////////////////////////////////////////////////////////////////

} // namespace Xn
