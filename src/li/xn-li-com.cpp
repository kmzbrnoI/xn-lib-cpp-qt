#include <QObject>
#include "xn-li-com.h"

namespace Xn {

XnLICom::XnLICom() {
	m_serialPort.setReadBufferSize(256);

	QObject::connect(&m_serialPort, SIGNAL(readyRead()), this, SLOT(sp_readyRead()));
	QObject::connect(&m_serialPort, SIGNAL(errorOccurred(QSerialPort::SerialPortError)), this,
	                 SLOT(sp_error(QSerialPort::SerialPortError)));
	QObject::connect(&m_serialPort, SIGNAL(aboutToClose()), this, SLOT(sp_aboutToClose()));
}

XnLICom::~XnLICom() {
	try {
		if (m_serialPort.isOpen())
			m_serialPort.close();
	}  catch (...) {
		// No exceptions in destructor
	}
}

void XnLICom::connect(const QString &portname, int32_t br, QSerialPort::FlowControl fc) {
	m_serialPort.setBaudRate(br);
	m_serialPort.setFlowControl(fc);
	m_serialPort.setPortName(portname);

	const bool success = m_serialPort.open(QIODevice::ReadWrite);
	if (!success)
		throw EOpenError(m_serialPort.errorString());
	emit onOpened();
}

void XnLICom::disconnect() {
	m_serialPort.close();
}

void XnLICom::send(QByteArray data) {
	qint64 sent = m_serialPort.write(data);
	if (sent == -1 || sent != data.size())
		throw EWriteError("No data could we written!");
}

bool XnLICom::connected() const {
	return m_serialPort.isOpen();
}

void XnLICom::sp_readyRead() {
	emit onReceived(m_serialPort.readAll());
}

void XnLICom::sp_error(QSerialPort::SerialPortError serialPortError) {
	if (serialPortError != QSerialPort::NoError)
		emit onError(m_serialPort.errorString());
}

void XnLICom::sp_aboutToClose() {
	emit onClosed();
}

} // namespace Xn
