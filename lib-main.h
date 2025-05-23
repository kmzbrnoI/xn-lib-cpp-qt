#ifndef LIB_MAIN_H
#define LIB_MAIN_H

#include <memory>
#include <QApplication>
#include <QMainWindow>

#include "ui_config-window.h"
#include "xn.h"
#include "lib-events.h"
#include "settings.h"

namespace Xn {

///////////////////////////////////////////////////////////////////////////////

const QString _DEFAULT_CONFIG_FILENAME = "trakce-xn.ini";

class ConfigWindow : public QMainWindow {
	Q_OBJECT
public:
	Ui::MainWindow ui;
	ConfigWindow(QWidget *parent = nullptr) : QMainWindow(parent) { ui.setupUi(this); }
};

class LibMain : public QObject {
	Q_OBJECT
public:
	XpressNet xn;
	ConfigWindow form;
	XnEvents events;
	Settings s;
	QString config_filename = "";
	unsigned int api_version = 0x0001;
	bool gui_config_changing = false;
	bool opening = false;
	unsigned int li_ver_hw = 0, li_ver_sw = 0;

	LibMain(QObject*);
	~LibMain() override;

	void guiInit();
	void fillConnectionsCbs();
	void fillPortCb();
	void guiOnOpen();
	void guiOnClose();

	void log(const QString &msg, LogLevel loglevel);
	void xnSetConfig();

private slots:
	void b_serial_refresh_handle();
	void cb_connections_changed(int);
	void cb_interface_type_changed(int);
	void b_info_update_handle();
	void b_li_addr_set_handle();

	void xnOnLog(QString message, Xn::LogLevel loglevel);
	void xnOnError(QString error);
	void xnOnConnect();
	void xnOnDisconnect();
	void xnOnLocoStolen(Xn::LocoAddr);
	void xnOnTrkStatusChanged(Xn::TrkStatus);

private:
	void xnGotLIVersion(void *, unsigned hw, unsigned sw);
	void xnOnLIVersionError(void *, void *);
	void xnOnLIAddrError(void *, void *);
	void xnOnCsVersionError(void *, void *);
	void xnOnCSStatusError(void *, void *);
	void xnGotCSVersion(void *, unsigned major, unsigned minor, uint8_t id);
	void xnGotLIAddress(void *, unsigned addr);

	void userLiAddrSet();
	void userLiAddrSetErr();

	void getLIVersion();
	void getCSVersion();
	void getCSStatus();
};

///////////////////////////////////////////////////////////////////////////////

// Dirty magic for Qt's event loop
// This class should be created first
struct AppThread {
	AppThread() {
		if (qApp == nullptr) {
			int argc = 0;
			auto* app = new QApplication(argc, nullptr);
			QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection);
			app->exec();
		}
	}
};

extern AppThread main_thread;
extern LibMain lib;

} // namespace Xn

#endif
