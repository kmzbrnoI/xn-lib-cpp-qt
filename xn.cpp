#include "xn.h"

/* Global definitions & helpers of XpressNet class. Specific functions that
 * do the real work are places in xn-*.cpp (logically divided into multiple
 * shorter source codes.
 */

namespace Xn {

XpressNet::XpressNet(QObject *parent) : QObject(parent) {
	m_serialPort.setReadBufferSize(256);
	m_lastSent = QDateTime::currentDateTime();

	QObject::connect(&m_serialPort, SIGNAL(readyRead()), this, SLOT(handleReadyRead()));
	QObject::connect(&m_serialPort, SIGNAL(errorOccurred(QSerialPort::SerialPortError)), this,
	                 SLOT(handleError(QSerialPort::SerialPortError)));

	QObject::connect(&m_hist_timer, SIGNAL(timeout()), this, SLOT(m_hist_timer_tick()));
	m_out_timer.setInterval(_OUT_TIMER_INTERVAL);
	QObject::connect(&m_out_timer, SIGNAL(timeout()), this, SLOT(m_out_timer_tick()));
	QObject::connect(&m_serialPort, SIGNAL(aboutToClose()), this, SLOT(sp_about_to_close()));
}

void XpressNet::sp_about_to_close() {
	m_hist_timer.stop();
	m_out_timer.stop();
	while (!m_hist.empty()) {
		if (nullptr != m_hist.front().callback_err)
			m_hist.front().callback_err->func(this, m_hist.front().callback_err->data);
		m_hist.pop_front();
	}
	while (!m_out.empty()) {
		if (nullptr != m_out.front().callback_err)
			m_out.front().callback_err->func(this, m_out.front().callback_err->data);
		m_out.pop_front();
	}
	m_trk_status = TrkStatus::Unknown;

	log("Disconnected", LogLevel::Info);
}

void XpressNet::log(const QString &message, const LogLevel loglevel) {
	if (loglevel <= this->loglevel)
        emit onLog(message, loglevel);
}

void XpressNet::handleError(QSerialPort::SerialPortError serialPortError) {
	if (serialPortError != QSerialPort::NoError)
        emit onError(m_serialPort.errorString());
}

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

bool XpressNet::liAcknowledgesSetAccState() const {
	return (this->m_liType == LIType::uLI || this->m_liType == LIType::LIUSBEth);
}

LIType XpressNet::liType() const { return this->m_liType; }

LIType interface(const QString &name) {
	if (name == "LI101")
		return Xn::LIType::LI101;
	if (name == "uLI")
		return Xn::LIType::uLI;
	if (name == "LI-USB-Ethernet")
		return Xn::LIType::LIUSBEth;
	return Xn::LIType::LI100;
}

QString interfaceName(const LIType &type) {
	if (type == LIType::LI101)
		return "LI101";
	if (type == LIType::uLI)
		return "uLI";
	if (type == LIType::LIUSBEth)
		return "LI-USB-Ethernet";
	return "LI100";
}

QString flowControlToStr(QSerialPort::FlowControl fc) {
	if (fc == QSerialPort::FlowControl::HardwareControl)
		return "hardware";
	if (fc == QSerialPort::FlowControl::SoftwareControl)
		return "software";
	if (fc == QSerialPort::FlowControl::NoFlowControl)
		return "no";
	return "unknown";
}

} // namespace Xn
