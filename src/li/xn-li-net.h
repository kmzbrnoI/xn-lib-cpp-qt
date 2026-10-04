#ifndef XN_LI_ETH_H
#define XN_LI_ETH_H

/* Interface to ethernet LI */

#include <QTcpSocket>

#include "xn-li.h"

namespace Xn {

class XnLINet : public XnLI {
	Q_OBJECT

private:
	QTcpSocket m_socket;
	bool connecting = false;

private slots:
	void socketConnected();
	void socketDisconnected();
	void socketReadyRead();
	void socketErrorOccured(QAbstractSocket::SocketError);

public:
	XnLINet();
	virtual ~XnLINet() = default;

	void connect(const QString &hostname, uint16_t port);
	void disconnect() override;
	void send(QByteArray data) override;
	bool connected() const override;

};

} // namespace Xn

#endif // XN_LI_ETH_H
