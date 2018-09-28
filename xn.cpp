#include "xn.h"


XpressNet::XpressNet(QString portname, QObject *parent)
	: QObject(parent) {
	m_serialPort.setBaudRate(9600);
	m_serialPort.setFlowControl(QSerialPort::FlowControl::HardwareControl);
	m_serialPort.setPortName(portname);
	m_serialPort.setReadBufferSize(256);

	if (!m_serialPort.open(QIODevice::ReadWrite))
		throw EOpenError(m_serialPort.errorString());

	QObject::connect(&m_hist_timer, SIGNAL(timeout()), this, SLOT(m_hist_timer_tick()));
	m_hist_timer.start(_HIST_CHECK_INTERVAL);
}

void XpressNet::send(std::vector<uint8_t> data) {
	QByteArray qdata(reinterpret_cast<const char*>(data.data()), data.size());

	uint8_t x = 0;
	for (uint8_t d : data)
		x ^= d;
	qdata.append(x);

	int sent = m_serialPort.write(qdata);

	if (sent == -1) {
		// TODO
	}
	if (sent != qdata.size()) {
		// TODO
	}
}

void XpressNet::send(XnCmd& cmd) {
	XnHistoryItem hist(cmd, QDateTime::currentDateTime(), 1, nullptr, nullptr); // TODO
	m_hist.push(hist);
	send(cmd.getBytes());
}

void XpressNet::send(XnCmd&& cmd) {
	XnCmd& cmd2(cmd);
	send(cmd2);
}

void XpressNet::send(XnHistoryItem& hist) {
	hist.no_sent++;
	hist.timeout = QDateTime::currentDateTime().addMSecs(_HIST_TIMEOUT);
	m_hist.push(hist);
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
			x ^= m_readData[i] & 0x7F;

		if (x != 0) {
			// XOR error
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
}

///////////////////////////////////////////////////////////////////////////////

void XpressNet::setTrkStatus(XnTrkStatus status) {
	if (status == XnTrkStatus::Off) {
		send(XnCmdOff());
	} else if (status == XnTrkStatus::On) {
		send(XnCmdOn());
	} else if (status == XnTrkStatus::Programming) {
		// TODO
	}
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

void XpressNet::log(QString message, XnLogLevel loglevel) {
	if (loglevel <= this->loglevel)
		onLog(message, loglevel);
}

///////////////////////////////////////////////////////////////////////////////
