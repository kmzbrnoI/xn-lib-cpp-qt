#include "xn.h"
#include "xn-win-com-discover.h"

/* Global definitions & helpers of XpressNet class. Specific functions that
 * do the real work are placed in xn-*.cpp (logically divided into multiple
 * shorter files.
 */

namespace Xn {

XpressNet::XpressNet(QObject *parent) : QObject(parent) {
	m_lastSent = QDateTime::currentDateTime();

	QObject::connect(&m_pending_timer, SIGNAL(timeout()), this, SLOT(m_pending_timer_tick()));
	m_out_timer.setInterval(m_config.outInterval);
	QObject::connect(&m_out_timer, SIGNAL(timeout()), this, SLOT(m_out_timer_tick()));
	QObject::connect(&m_keep_alive_timer, SIGNAL(timeout()), this, SLOT(m_keep_alive_timer_tick()));
}

XpressNet::~XpressNet() {
	try {
		if ((m_li) && (m_li->connected()))
			m_li->disconnect();
	}  catch (...) {
		// No exceptions in destructor
	}
}

void XpressNet::li_opened() {
	m_pending_timer.start(_PENDING_CHECK_INTERVAL);
	log("Connected", LogLevel::Info);
	emit onConnect();
}

void XpressNet::li_closed() {
	m_pending_timer.stop();
	m_out_timer.stop();
	while (!m_pending.empty()) {
		if (nullptr != m_pending.front().callback_err)
			m_pending.front().callback_err->func(this, m_pending.front().callback_err->data);
		m_pending.pop_front();
	}
	while (!m_out.empty()) {
		if (nullptr != m_out.front().callback_err)
			m_out.front().callback_err->func(this, m_out.front().callback_err->data);
		m_out.pop_front();
	}
	m_trk_status = TrkStatus::Unknown;

	m_keep_alive_timer.stop();

	log("Disconnected", LogLevel::Info);
	emit onDisconnect();
}

void XpressNet::log(const QString &message, const LogLevel loglevel) {
	if (loglevel <= this->loglevel)
		emit onLog(message, loglevel);
}

void XpressNet::li_error(QString message) {
	emit onError(message);
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
	return (m_liType == LIType::uLI || m_liType == LIType::LIUSBEth);
}

LIType XpressNet::liType() const { return m_liType; }

LIType liInterface(const QString &name) {
	if (name == "LI101")
		return Xn::LIType::LI101;
	if (name == "uLI")
		return Xn::LIType::uLI;
	if (name.startsWith("LI-USB-Ethernet"))
		return Xn::LIType::LIUSBEth;
	return Xn::LIType::LI100;
}

QString liInterfaceName(const LIType &type) {
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

std::vector<QSerialPortInfo> XpressNet::ports(LIType litype) {
	if (litype != LIType::uLI)
		throw EUnsupportedInterface("Cannot autodetect port for "+liInterfaceName(litype));

#ifdef Q_OS_WIN
	return winULIPorts();
#else
	std::vector<QSerialPortInfo> result;
	QList<QSerialPortInfo> ports(QSerialPortInfo::availablePorts());
	for (const QSerialPortInfo &info : ports)
		if (info.description().startsWith("uLI"))
			result.push_back(info);
	return result;
#endif
}

XNConfig XpressNet::config() const {
	return m_config;
}

void XpressNet::setConfig(const XNConfig config) {
	if ((config.outInterval < _OUT_TIMER_INTERVAL_MIN) || (config.outInterval > _OUT_TIMER_INTERVAL_MAX))
		throw EInvalidConfig("outInterval="+QString::number(config.outInterval)+" is out of range ["+
		      QString::number(_OUT_TIMER_INTERVAL_MIN)+"-"+QString::number(_OUT_TIMER_INTERVAL_MAX)+"]");

	m_config = config;
	m_out_timer.setInterval(m_config.outInterval);
}

QString XpressNet::liVersionToStr(unsigned version)
{
	return QString::number((version >> 4) & 0xF) + "." + QString::number(version & 0xF);
}

void XpressNet::liConnectSignals() {
	if (m_li) {
		QObject::connect(m_li.get(), SIGNAL(onReceived(QByteArray)), this, SLOT(li_received(QByteArray)));
		QObject::connect(m_li.get(), SIGNAL(onError(QString)), this, SLOT(li_error(QString)));
		QObject::connect(m_li.get(), SIGNAL(onOpened()), this, SLOT(li_opened()));
		QObject::connect(m_li.get(), SIGNAL(onClosed()), this, SLOT(li_closed()));
	}
}

void XpressNet::m_keep_alive_timer_tick() {
	if ((!this->connected()) || (!this->m_config.keepAlive))
		return;

	if (!m_anyCsReceived) {
		try {
			this->getCommandStationStatus(
				std::make_unique<Cb>([this](void*, void*) {
					this->m_anyCsReceived = false;
				}),
				std::make_unique<Cb>([this](void*, void*) {
					this->log("Disconnecting due to Keep Alive timeout", LogLevel::Error);
					this->disconnect();
				})
			);
		} catch (const QStrException &e) {
			log("Keep alive Get CS Status error: " + e.str(), LogLevel::Error);
			this->disconnect();
		}
	}

	this->m_anyCsReceived = false;
}

} // namespace Xn
