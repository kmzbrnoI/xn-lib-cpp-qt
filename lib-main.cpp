#include "lib-main.h"

namespace Xn {

AppThread main_thread;
LibMain lib(nullptr);

///////////////////////////////////////////////////////////////////////////////

LibMain::LibMain(QObject* parent) : QObject(parent) {
	xn.loglevel = LogLevel::Debug;

	QObject::connect(&xn, SIGNAL(onError(QString)), this, SLOT(xnOnError(QString)));
	QObject::connect(&xn, SIGNAL(onLog(QString, Xn::LogLevel)), this,
					 SLOT(xnOnLog(QString, Xn::LogLevel)));
	QObject::connect(&xn, SIGNAL(onConnect()), this, SLOT(xnOnConnect()));
	QObject::connect(&xn, SIGNAL(onDisconnect()), this, SLOT(xnOnDisconnect()));
	QObject::connect(&xn, SIGNAL(onLocoStolen(Xn::LocoAddr)), this, SLOT(xnOnLocoStolen(Xn::LocoAddr)));
	QObject::connect(&xn, SIGNAL(onTrkStatusChanged(Xn::TrkStatus)), this,
					 SLOT(xnOnTrkStatusChanged(Xn::TrkStatus)));

	this->config_filename = _DEFAULT_CONFIG_FILENAME;
	s.load(this->config_filename);
	this->guiInit();
	log("Library loaded.", LogLevel::Info);
}

LibMain::~LibMain() {
	try {
		if (xn.connected())
			xn.disconnect();
		if (this->config_filename != "")
			this->s.save(this->config_filename);
	} catch (...) {
		// No exceptions in destructor!
	}
}

///////////////////////////////////////////////////////////////////////////////

void LibMain::log(const QString &msg, LogLevel loglevel) {
	events.call(events.onLog, loglevel, msg);
}

///////////////////////////////////////////////////////////////////////////////
// Xn events:

void LibMain::xnOnLog(QString message, LogLevel loglevel) {
	if (this->opening && (message == "Not responded to command: LI Get Address" ||
						  message == "Not responded to command: Get Command station version"))
		loglevel = LogLevel::Warning;
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
	this->getLIVersion();
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

void LibMain::getLIVersion() {
	try {
		xn.getLIVersion(
			[this](void *s, unsigned hw, unsigned sw) { xnGotLIVersion(s, hw, sw); },
			std::make_unique<Cb>([this](void *s, void *d) { xnOnLIVersionError(s, d); })
		);
	} catch (const QStrException &e) {
		log("Get LI Version: " + e.str(), LogLevel::Error);
		events.call(events.onOpenError, "Get LI Version: " + e.str());
		xn.disconnect();
	}
}

void LibMain::xnGotLIVersion(void *, unsigned hw, unsigned sw) {
	QString version = "HW:" + QString::number(hw) + ", SW: " + QString::number(sw);
	this->li_ver_hw = hw;
	this->li_ver_sw = sw;

	form.ui.l_li_version->setText(version);
	form.ui.l_info_datetime->setText(QTime::currentTime().toString("hh:mm:ss"));

	try {
		xn.getLIAddress(
			[this](void *s, unsigned addr) { xnGotLIAddress(s, addr); },
			std::make_unique<Cb>([this](void *s, void *d) { xnOnLIAddrError(s, d); })
		);
	} catch (const QStrException &e) {
		log("Get LI Address: " + e.str(), LogLevel::Error);
		events.call(events.onOpenError, "Get LI Address: " + e.str());
		xn.disconnect();
	}
}

void LibMain::xnOnLIVersionError(void *, void *) {
	log("Get LI Version: no response!", LogLevel::Error);
	events.call(events.onOpenError, "Get LI Version: no response!");
	xn.disconnect();
}

void LibMain::xnGotLIAddress(void *, unsigned addr) {
	form.ui.sb_li_addr->setValue(addr);
	form.ui.l_info_datetime->setText(QTime::currentTime().toString("hh:mm:ss"));
	this->getCSVersion();
}

void LibMain::xnOnLIAddrError(void *, void *) {
	log("Unable to get LI address, ignoring!", LogLevel::Warning);
	form.ui.l_info_datetime->setText(QTime::currentTime().toString("hh:mm:ss"));
	this->getCSVersion();
}

void LibMain::getCSVersion() {
	try {
		xn.getCommandStationVersion(
			[this](void *s, unsigned major, unsigned minor, uint8_t id) {
				xnGotCSVersion(s, major, minor, id);
			},
			std::make_unique<Cb>([this](void *s, void *d) { xnOnCsVersionError(s, d); })
		);
	} catch (const QStrException &e) {
		log("Get CS Version: " + e.str(), LogLevel::Error);
		events.call(events.onOpenError, "Get CS Version: " + e.str());
		xn.disconnect();
	}
}

void LibMain::xnGotCSVersion(void *, unsigned major, unsigned minor, uint8_t id) {
	QString version = QString::number(major) + "." + QString::number(minor);
	form.ui.l_cs_version->setText(version);
	form.ui.l_cs_id->setText(QString::number(id));
	form.ui.l_info_datetime->setText(QTime::currentTime().toString("hh:mm:ss"));
	this->getCSStatus();
}

void LibMain::xnOnCsVersionError(void *, void *) {
	log("Command station version not received, ignoring!", LogLevel::Warning);
	form.ui.l_cs_version->setText("Nelze zjistit");
	form.ui.l_cs_id->setText("Nelze zjistit");
	form.ui.l_info_datetime->setText(QTime::currentTime().toString("hh:mm:ss"));
	this->getCSStatus();
}

void LibMain::getCSStatus() {
	try {
		xn.getCommandStationStatus(
			nullptr,
			std::make_unique<Cb>([this](void *s, void *d) { xnOnCSStatusError(s, d); })
		);
	} catch (const QStrException &e) {
		log("Get CS Status: " + e.str(), LogLevel::Error);
		events.call(events.onOpenError, "Get CS Status: " + e.str());
		xn.disconnect();
	}
}

void LibMain::xnOnCSStatusError(void *, void *) {
	log("Get CS Status: no response!", LogLevel::Error);
	events.call(events.onOpenError, "Get CS Status: no response!");
	xn.disconnect();
}

} // namespace Xn
