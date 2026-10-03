#ifndef XN_LOCO_ADDR
#define XN_LOCO_ADDR

/*
This file defines XpressNET locomotive address.
*/

#include <functional>
#include <memory>

#include "q-str-exception.h"

namespace Xn {

struct EInvalidAddr : public QStrException {
	EInvalidAddr(const QString str) : QStrException(str) {}
};

struct LocoAddr {
	uint16_t addr;

	LocoAddr(uint16_t addr) : addr(addr) {
		if (addr > 9999)
			throw EInvalidAddr("Invalid loco address!");
	}

	LocoAddr(uint8_t lo, uint8_t hi) {
		if (hi == 0)
			this->addr = lo;
		else
			this->addr = lo + ((hi-0xC0) << 8);
		if (this->addr > 9999)
			throw EInvalidAddr("Invalid loco address!");
	}

	uint8_t lo() const { return addr & 0xFF; }
	uint8_t hi() const { return ((addr >> 8) & 0xFF) + (addr < 100 ? 0 : 0xC0); }

	operator uint16_t() const { return addr; }
	operator QString() const { return QString::number(addr); }
};

} // namespace Xn

#endif
