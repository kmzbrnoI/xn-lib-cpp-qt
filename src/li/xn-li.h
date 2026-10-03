#ifndef XN_LI_H
#define XN_LI_H

/* LI (Lenz Interface) abstract class representing all
   physical hardware interfaces */

#include <QObject>

#include "../q-str-exception.h"

namespace Xn {

struct EOpenError : public QStrException {
	EOpenError(const QString str) : QStrException(str) {}
};
struct EWriteError : public QStrException {
	EWriteError(const QString str) : QStrException(str) {}
};

class XnLI : public QObject {
	Q_OBJECT

public:
	XnLI() : QObject(nullptr) {}
	virtual ~XnLI() = default;

	// connect() is intentionally not here - arguments vary for children
	virtual void disconnect() = 0;
	virtual void send(QByteArray data) = 0;
	virtual bool connected() const = 0;

signals:
	void onReceived(QByteArray data);
	void onClosed();
	void onError(QString error);

};

} // namespace Xn

#endif // XN_LI_H
