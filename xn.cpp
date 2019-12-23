#include <sstream>

/* Global definitions & helpers of XpressNet class. Specific functions that
 * do the real work are places in xn-*.cpp (logically divided into multiple
 * shorter source codes.
 */

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

void XpressNet::sp_about_to_close() {
	m_hist_timer.stop();
	while (!m_hist.empty())
		m_hist.pop_front(); // Should we call error events?
	while (!m_out.empty())
		m_out.pop(); // Should we call error events?
	m_trk_status = TrkStatus::Unknown;

	log("Disconnected", LogLevel::Info);
}

template <typename Target>
bool XpressNet::is(const HistoryItem &h) {
	return (dynamic_cast<const Target *>(h.cmd.get()) != nullptr);
}

void XpressNet::log(const QString &message, const LogLevel loglevel) {
	if (loglevel <= this->loglevel)
		onLog(message, loglevel);
}

template <typename DataT, typename ItemType>
QString XpressNet::dataToStr(DataT data, size_t len) {
	QString out;
	size_t i = 0;
	for (auto d = data.begin(); (d != data.end() && (len == 0 || i < len)); d++, i++)
		out += QString("0x%1 ").arg(static_cast<ItemType>(*d), 2, 16, QLatin1Char('0'));

	return out;
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
