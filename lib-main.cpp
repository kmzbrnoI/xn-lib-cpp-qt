#include "lib-main.h"

namespace Xn {

AppThread main_thread;
LibMain lib;

LIType LibMain::interface(const QString &name) const {
	if (name == "LI101")
		return Xn::LIType::LI101;
	if (name == "uLI")
		return Xn::LIType::uLI;
	if (name == "LI-USB-Ethernet")
		return Xn::LIType::LIUSBEth;
	return Xn::LIType::LI100;
}

void LibMain::log(const QString &msg, LogLevel loglevel) {
	events.call(events.onLog, loglevel, msg);
}

} // namespace Xn
