#include <sstream>

#include "xn.h"

namespace Xn {

void XpressNet::hist_ok() {
	if (m_hist.empty()) {
		log("History buffer underflow!", LogLevel::Warning);
		return;
	}

	HistoryItem hist = std::move(m_hist.front());
	m_hist.pop_front();
	if (nullptr != hist.callback_ok)
		hist.callback_ok->func(this, hist.callback_ok->data);
	if (!m_out.empty())
		send_next_out();
}

void XpressNet::hist_err() {
	if (m_hist.empty()) {
		log("History buffer underflow!", LogLevel::Warning);
		return;
	}

	HistoryItem hist = std::move(m_hist.front());
	m_hist.pop_front();

	log("Not responded to command: " + hist.cmd->msg(), LogLevel::Error);

	if (nullptr != hist.callback_err)
		hist.callback_err->func(this, hist.callback_err->data);
	if (!m_out.empty())
		send_next_out();
}

void XpressNet::hist_send() {
	HistoryItem hist = std::move(m_hist.front());
	m_hist.pop_front();

	for(const HistoryItem &newer : m_hist) {
		if (hist.cmd->conflict(*(newer.cmd)) || newer.cmd->conflict(*(hist.cmd))) {
			log("Not sending again, conflict: " + hist.cmd->msg() + ", " + newer.cmd->msg(),
			    LogLevel::Warning);
			if (nullptr != hist.callback_err)
				hist.callback_err->func(this, hist.callback_err->data);
			return;
		}
	}

	log("Sending again: " + hist.cmd->msg(), LogLevel::Warning);

	try {
		send(std::move(hist));
	} catch (...) {}
}

void XpressNet::m_hist_timer_tick() {
	if (!m_serialPort.isOpen()) {
		while (!m_hist.empty())
			hist_err();
	}

	if (m_hist.empty())
		return;

	if (m_hist.front().timeout < QDateTime::currentDateTime()) {
		if (m_hist.front().no_sent >= _HIST_SEND_MAX)
			hist_err();
		else
			hist_send();
	}
}

void XpressNet::send_next_out() {
	HistoryItem out = std::move(m_out.front());
	m_out.pop();
	out.no_sent = 0;
	send(std::move(out));
}

void XpressNet::histClear() {
	size_t hist_size = m_hist.size();
	for (size_t i = 0; i < hist_size; ++i)
		hist_err(); // can add next items to history!
}

} // namespace Xn
