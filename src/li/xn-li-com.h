#ifndef XN_LI_COM_H
#define XN_LI_COM_H

/* Interface to COM port LI. */

#include <QSerialPort>

#include "xn-li.h"

namespace Xn {

class XnLICom : public XnLI {
	Q_OBJECT

private:
	QSerialPort m_serialPort;

private slots:
	void sp_readyRead();
	void sp_error(QSerialPort::SerialPortError);
	void sp_aboutToClose();

public:
	XnLICom();
	virtual ~XnLICom();

	void connect(const QString &portname, int32_t br, QSerialPort::FlowControl fc);
	void disconnect() override;
	void send(QByteArray data) override;
	bool connected() const override;
};

} // namespace Xn

#endif // XN_LI_COM_H
