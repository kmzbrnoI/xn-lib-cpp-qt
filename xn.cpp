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
	while (!m_hist.empty())
		m_hist.pop_front(); // Should we call error events?
	while (!m_out.empty())
		m_out.pop_front(); // Should we call error events?
	m_trk_status = TrkStatus::Unknown;

	log("Disconnected", LogLevel::Info);
}

void XpressNet::log(const QString &message, const LogLevel loglevel) {
	if (loglevel <= this->loglevel)
		onLog(message, loglevel);
}

void XpressNet::handleError(QSerialPort::SerialPortError serialPortError) {
	if (serialPortError != QSerialPort::NoError)
		onError(m_serialPort.errorString());
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

} // namespace Xn
