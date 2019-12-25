#ifndef LIB_MAIN_H
#define LIB_MAIN_H

#include <memory>
#include <QApplication>

#include "config-window.h"
#include "xn.h"
#include "lib-events.h"
#include "settings.h"

namespace Xn {

///////////////////////////////////////////////////////////////////////////////

class LibMain : public QObject {
	Q_OBJECT
public:
	XpressNet xn;
	ConfigWindow form;
	XnEvents events;
	Settings s;
	unsigned int api_version = 0x0001;
	bool gui_config_changing = false;

	void guiInit();
	void fillConnectionsCbs();
	void fillPortCb();
	void guiOnOpen();
	void guiOnClose();

	LIType interface(const QString &name) const;
	void log(const QString &msg, LogLevel loglevel);

private slots:
	void b_serial_refresh_handle();
	void cb_connections_changed(int);

private:

};

///////////////////////////////////////////////////////////////////////////////

// Dirty magic for Qt's event loop
// This class should be created first
class AppThread {
	std::unique_ptr<QApplication> app;
	int argc {0};

public:
	AppThread() {
		app = std::make_unique<QApplication>(argc, nullptr);
		QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection);
		app->exec();
	}
};

extern AppThread main_thread;
extern LibMain lib;

} // namespace Xn

#endif
