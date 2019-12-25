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

using TrkStdNotifyEvent = void CALL_CONV (*)(const void *sender);
using TrkStatusChangedEv = void(*)(const void *sender, int trkStatus);
using TrkLogEvent = void CALL_CONV (*)(const void *sender, int loglevel, const uint16_t *msg);
using TrkLocoEv = void(*)(const void *sender, uint16_t addr);

template <typename F>
struct EventData {
	F func = nullptr;
	bool defined() const { return this->func != nullptr; }
};

struct XnEvents {
	EventData<TrkStdNotifyEvent> beforeOpen;
	EventData<TrkStdNotifyEvent> afterOpen;
	EventData<TrkStdNotifyEvent> beforeClose;
	EventData<TrkStdNotifyEvent> afterClose;

	EventData<TrkLogEvent> onLog;
	EventData<TrkStatusChangedEv> onTrkStatusChanged;
	EventData<TrkLocoEv> onLocoStolen;

	void call(const EventData<TrkStdNotifyEvent> &e) const {
		if (e.defined())
			e.func(this);
	}
	void call(const EventData<TrkLogEvent> &e, LogLevel loglevel, const QString &msg) const {
		if (e.defined())
			e.func(this, static_cast<int>(loglevel), msg.utf16());
	}
	void call(const EventData<TrkStatusChangedEv> &e, TrkStatus status) const {
		if (e.defined())
			e.func(this, static_cast<int>(status));
	}

	template <typename F>
	static void bind(EventData<F> &event, const F &func) {
		event.func = func;
	}
};

} // namespace Xn

#endif
