#ifndef LIB_MAIN_H
#define LIB_MAIN_H

#include <memory>
#include <QApplication>

#include "config-window.h"
#include "xn.h"

namespace Xn {

///////////////////////////////////////////////////////////////////////////////

class LibMain : public QObject {
	Q_OBJECT
public:
	XpressNet xn;
	ConfigWindow form;

private slots:

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
