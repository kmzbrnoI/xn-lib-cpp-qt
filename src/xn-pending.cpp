#include "xn.h"

/* Handling of the pending commands = sent commands with no response from the command station
 * yes (e.g. resending data with no response etc.).
 */

namespace Xn {

void XpressNet::pending_ok() {
	if (m_pending.empty()) {
		log("Pending buffer underflow!", LogLevel::Warning);
		return;
	}

	PendingItem pending = std::move(m_pending.front());
	m_pending.pop_front();
	if (nullptr != pending.callback_ok)
		pending.callback_ok->func(this, pending.callback_ok->data);
	if (!m_out.empty())
		send_next_out();
}

void XpressNet::pending_err(bool _log) {
	if (m_pending.empty()) {
		log("Pending buffer underflow!", LogLevel::Warning);
		return;
	}

	PendingItem pending = std::move(m_pending.front());
	m_pending.pop_front();

	if (_log)
		log("Not responded to command: " + pending.cmd->msg(), LogLevel::Error);

	if (nullptr != pending.callback_err)
		pending.callback_err->func(this, pending.callback_err->data);
	if (!m_out.empty())
		send_next_out();
}

void XpressNet::pending_send() {
	PendingItem pending = std::move(m_pending.front());
	m_pending.pop_front();

	// to_send guarantees us that conflict can never occur in pending buffer
	// we just check conflict in out buffer

	if (this->conflictWithOut(*(pending.cmd))) {
		log("Not sending again, conflict: " + pending.cmd->msg(), LogLevel::Warning);
		if (nullptr != pending.callback_err)
			pending.callback_err->func(this, pending.callback_err->data);
		if (!m_out.empty())
			send_next_out();
		return;
	}

	log("Sending again: " + pending.cmd->msg(), LogLevel::Warning);

	try {
		to_send(std::move(pending), true);
	} catch (...) {}
}

void XpressNet::m_pending_timer_tick() {
	if (!m_serialPort.isOpen()) {
		while (!m_pending.empty())
			pending_err();
	}

	if (m_pending.empty())
		return;

	if (m_pending.front().timeout < QDateTime::currentDateTime()) {
		if (m_pending.front().no_sent >= _PENDING_SEND_MAX)
			pending_err();
		else
			pending_send();
	}
}

void XpressNet::pendingClear() {
	size_t pending_size = m_pending.size();
	for (size_t i = 0; i < pending_size; ++i)
		pending_err(); // can add next items to pending!
}

bool XpressNet::conflictWithPending(const Cmd &cmd) const {
	for (const PendingItem &pending : m_pending)
		if (pending.cmd->conflict(cmd) || cmd.conflict(*(pending.cmd)))
			return true;
	return false;
}

bool XpressNet::conflictWithOut(const Cmd &cmd) const {
	for (const PendingItem &out : m_out)
		if (out.cmd->conflict(cmd) || cmd.conflict(*(out.cmd)))
			return true;
	return false;
}

} // namespace Xn
