#ifndef XN_LI_ETH_H
#define XN_LI_ETH_H

/* Interface to ethernet LI */

#include "xn-li.h"

namespace Xn {

class XnLINet : public XnLI {
	Q_OBJECT

private:


public:
	XnLINet();
	virtual ~XnLINet();

	void connect(const QString &hostname, uint16_t port);
	void disconnect() override;
	void send(QByteArray data) override;
	bool connected() const override;

};

} // namespace Xn

#endif // XN_LI_ETH_H
