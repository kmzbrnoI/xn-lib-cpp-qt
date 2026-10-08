#ifndef XN_LI_ETH_H
#define XN_LI_ETH_H

/* Interface to ethernet LI */

#include <QTcpSocket>
#include <QTimer>

#include "xn-li.h"

namespace Xn {

class XnLINet : public XnLI {
	Q_OBJECT

private:
	static constexpr size_t _CONNECTING_TIMEOUT_S = 5;
	static constexpr size_t _KEEP_ALIVE_INTERVAL_MS = 5000;
	static constexpr size_t _KEEP_ALIVE_TIMEOUT_MS = 10000;

	QTcpSocket m_socket;
	QTimer m_tConnecting;
	bool connecting = false;

	void setSocketKeepAliveTime(QTcpSocket &socket, size_t timeoutMs, size_t intervalMs);

private slots:
	void socketConnected();
	void socketDisconnected();
	void socketReadyRead();
	void socketErrorOccured(QAbstractSocket::SocketError);
	void connectingTimeout();

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
