#ifndef LIBAPI_H
#define LIBAPI_H

#include <memory>
#include <QApplication>

#include "xn.h"

#if defined(XN_SHARED_LIBRARY)
#define XN_SHARED_EXPORT Q_DECL_EXPORT
#else
#define XN_SHARED_EXPORT Q_DECL_IMPORT
#endif

#ifdef Q_OS_WIN
#define CALL_CONV __stdcall
#else
#define CALL_CONV
#endif

namespace Xn {

extern "C" {
XN_SHARED_EXPORT int CALL_CONV connect();
XN_SHARED_EXPORT int CALL_CONV disconnect();
XN_SHARED_EXPORT bool CALL_CONV connected();
}

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
extern XpressNet xn;

} // namespace Xn

#endif
