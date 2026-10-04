#include "xn-li-net.h"

namespace Xn {

XnLINet::XnLINet() {
	QObject::connect(&m_socket, SIGNAL(connected()), this, SLOT(socketConnected()));
	QObject::connect(&m_socket, SIGNAL(disconnected()), this, SLOT(socketDisconnected()));
	QObject::connect(&m_socket, SIGNAL(readyRead()), this, SLOT(socketReadyRead()));

	QObject::connect(&m_socket, SIGNAL(errorOccurred(QAbstractSocket::SocketError)),
	                 this, SLOT(socketErrorOccured(QAbstractSocket::SocketError)));

	//m_socket.
}

void XnLINet::connect(const QString &hostname, uint16_t port) {
	this->connecting = true;
	try {
		this->m_socket.connectToHost(hostname, port);
	} catch (...) {
		throw EOpenError(this->m_socket.errorString());
		this->connecting = false;
	}
}

void XnLINet::disconnect() {
	this->m_socket.abort();
}

void XnLINet::send(QByteArray data) {
	this->m_socket.write(data);
}

bool XnLINet::connected() const {
	return this->m_socket.state() == QAbstractSocket::SocketState::ConnectedState;
}

void XnLINet::socketConnected() {
	this->connecting = false;
	emit onOpened();
}

void XnLINet::socketDisconnected() {
	this->connecting = false;
	emit onClosed();
}

void XnLINet::socketReadyRead() {
	QByteArray data = this->m_socket.readAll();
	if (data.size() > 0)
		emit onReceived(data);
}

void XnLINet::socketErrorOccured(QAbstractSocket::SocketError) {
	emit onError(this->m_socket.errorString());
	if (this->connecting) {
		this->connecting = false;
		emit onClosed();
	}
}

} // namespace Xn
