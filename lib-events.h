#ifndef LIB_EVENTS_H
#define LIB_EVENTS_H

#include <QString>
#include <cstdint>

#include "lib-api-common-def.h"
#include "xn.h"

/* This file provides storage & calling capabilities of callbacks from the
 * library back to the hJOPserver.
 */

namespace Xn {

using TrkStdNotifyEvent = void CALL_CONV (*)(const void *sender, void *data);
using TrkStatusChangedEv = void CALL_CONV (*)(const void *sender, void *data, int trkStatus);
using TrkLogEv = void CALL_CONV (*)(const void *sender, void *data, int loglevel, const uint16_t *msg);
using TrkLocoEv = void CALL_CONV (*)(const void *sender, void *data, uint16_t addr);

template <typename F>
struct EventData {
	F func = nullptr;
	void *data = nullptr;
	bool defined() const { return this->func != nullptr; }
};

struct XnEvents {
	EventData<TrkStdNotifyEvent> beforeOpen;
	EventData<TrkStdNotifyEvent> afterOpen;
	EventData<TrkStdNotifyEvent> beforeClose;
	EventData<TrkStdNotifyEvent> afterClose;

	EventData<TrkLogEv> onLog;
	EventData<TrkStatusChangedEv> onTrkStatusChanged;
	EventData<TrkLocoEv> onLocoStolen;

	void call(const EventData<TrkStdNotifyEvent> &e) const {
		if (e.defined())
			e.func(this, e.data);
	}
	void call(const EventData<TrkLogEv> &e, LogLevel loglevel, const QString &msg) const {
		if (e.defined())
			e.func(this, e.data, static_cast<int>(loglevel), msg.utf16());
	}
	void call(const EventData<TrkStatusChangedEv> &e, TrkStatus status) const {
		if (e.defined())
			e.func(this, e.data, static_cast<int>(status));
	}
	void call(const EventData<TrkLocoEv> &e, LocoAddr addr) const {
		if (e.defined())
			e.func(this, e.data, addr.addr);
	}

	template <typename F>
	static void bind(EventData<F> &event, const F &func, void *const data) {
		event.func = func;
		event.data = data;
	}
};

} // namespace Xn

#endif
