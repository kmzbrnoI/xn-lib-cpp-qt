#include "lib-main.h"

namespace Xn {

AppThread main_thread;
LibMain lib;

///////////////////////////////////////////////////////////////////////////////

LibMain::LibMain() {
	xn.loglevel = LogLevel::Debug;

	QObject::connect(&xn, SIGNAL(onError(QString)), this, SLOT(xnOnError(QString)));
	QObject::connect(&xn, SIGNAL(onLog(QString, Xn::LogLevel)), this,
					 SLOT(xnOnLog(QString, Xn::LogLevel)));
	QObject::connect(&xn, SIGNAL(onConnect()), this, SLOT(xnOnConnect()));
	QObject::connect(&xn, SIGNAL(onDisconnect()), this, SLOT(xnOnDisconnect()));
	QObject::connect(&xn, SIGNAL(onLocoStolen(LocoAddr)), this, SLOT(xnOnLocoStolen(LocoAddr)));
	QObject::connect(&xn, SIGNAL(onTrkStatusChanged(Xn::TrkStatus)), this,
					 SLOT(xnOnTrkStatusChanged(Xn::TrkStatus)));

	s.load(_CONFIG_FILENAME);
	this->guiInit();
	log("Library loaded.", LogLevel::Info);
}

LibMain::~LibMain() {
	try {
		if (xn.connected())
			xn.disconnect();
	} catch (...) {
		// No exceptions in destructor!
	}
}

///////////////////////////////////////////////////////////////////////////////

LIType LibMain::interface(const QString &name) const {
	if (name == "LI101")
		return Xn::LIType::LI101;
	if (name == "uLI")
		return Xn::LIType::uLI;
	if (name == "LI-USB-Ethernet")
		return Xn::LIType::LIUSBEth;
	return Xn::LIType::LI100;
}

void LibMain::log(const QString &msg, LogLevel loglevel) {
	events.call(events.onLog, loglevel, msg);
}

///////////////////////////////////////////////////////////////////////////////
// Xn events:

void LibMain::xnOnLog(QString message, LogLevel loglevel) {
	this->events.call(this->events.onLog, loglevel, message);
}

void LibMain::xnOnError(QString error) {
	// Xn error is considered fatal -> close device
	log("XN error: " + error, LogLevel::Error);
	if (xn.connected())
		xn.disconnect();
}

void LibMain::xnOnConnect() {
	lib.guiOnOpen();
	this->opening = true;

	try {
		xn.getLIVersion(
		    [this](void *s, unsigned hw, unsigned sw) { xnGotLIVersion(s, hw, sw); },
		    std::make_unique<Cb>([this](void *s, void *d) { xnOnLIVersionError(s, d); })
		);
	} catch (const QStrException &e) {
		log("Get LI Version: " + e.str(), LogLevel::Error);
		xn.disconnect();
	}
}

void LibMain::xnOnDisconnect() {
	this->opening = false;
	this->guiOnClose();
	this->events.call(this->events.afterClose);
}

void LibMain::xnOnLocoStolen(LocoAddr addr) {
	this->events.call(this->events.onLocoStolen, addr);
}

void LibMain::xnOnTrkStatusChanged(TrkStatus trkStatus) {
	this->events.call(this->events.onTrkStatusChanged, trkStatus);

	if (this->opening) {
		this->opening = false;
		this->events.call(this->events.afterOpen);
	}
}

void LibMain::xnOnLIVersionError(void *, void *) {
	log("Get LI Version: no response!", LogLevel::Error);
	xn.disconnect();
}

void LibMain::xnOnCSStatusError(void *, void *) {
	log("Get CS Status: no response!", LogLevel::Error);
	xn.disconnect();
}

void LibMain::xnGotLIVersion(void *, unsigned hw, unsigned sw) {
	QString version = "HW:" + QString::number(hw) + ", SW: " + QString::number(sw);
	log("Got LI version: " + version, LogLevel::Info);
	this->li_ver_hw = hw;
	this->li_ver_sw = sw;

	form.ui.l_li_version->setText(version);
	form.ui.l_info_datetime->setText(QTime::currentTime().toString("hh:mm:ss"));

	try {
		xn.getLIAddress(
		    [this](void *s, unsigned addr) { xnGotLIAddress(s, addr); }
		);
		xn.getCommandStationVersion(
		    [this](void *s, unsigned major, unsigned minor, uint8_t id) {
		        xnGotCSVersion(s, major, minor, id);
		    }
		);
		xn.getCommandStationStatus(
		    nullptr,
		    std::make_unique<Cb>([this](void *s, void *d) { xnOnCSStatusError(s, d); })
		);
	} catch (const QStrException &e) {
		log("Get CS Status: " + e.str(), LogLevel::Error);
		xn.disconnect();
	}
}

void LibMain::xnGotCSVersion(void *, unsigned major, unsigned minor, uint8_t id) {
	QString version = QString::number(major) + "." + QString::number(minor);
	log("Got command station version: " + version + ", ID: " + QString::number(id),
	    LogLevel::Info);
	form.ui.l_cs_version->setText(version);
	form.ui.l_cs_id->setText(QString::number(id));
	form.ui.l_info_datetime->setText(QTime::currentTime().toString("hh:mm:ss"));
}

void LibMain::xnGotLIAddress(void *, unsigned addr) {
	log("Got LI address: " + QString::number(addr), LogLevel::Info);
	form.ui.sb_li_addr->setValue(addr);
	form.ui.l_info_datetime->setText(QTime::currentTime().toString("hh:mm:ss"));
}

} // namespace Xn
