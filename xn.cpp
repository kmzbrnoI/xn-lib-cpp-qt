#include "xn.h"


XpressNet::XpressNet(QString portname, QObject *parent)
	: QObject(parent) {
	m_serialPort.setBaudRate(9600);
	m_serialPort.setFlowControl(QSerialPort::FlowControl::HardwareControl);
	m_serialPort.setPortName(portname);
	m_serialPort.setReadBufferSize(256);

	if (!m_serialPort.open(QIODevice::ReadWrite))
		throw EOpenError(m_serialPort.errorString());
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
