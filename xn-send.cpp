#include "xn.h"

/* Sending data to XpressNet. */

namespace Xn {

void XpressNet::send(MsgType data) {
	uint8_t x = 0;
	for (uint8_t d : data)
		x ^= d;
	data.push_back(x);

	if (this->m_liType == LIType::LIUSBEth) {
		data.emplace(data.begin(), 0xFE);
		data.emplace(data.begin(), 0xFF);
	}

	log("PUT: " + dataToStr<MsgType, uint8_t>(data), LogLevel::RawData);
	QByteArray qdata(reinterpret_cast<const char *>(data.data()), data.size());

	qint64 sent = m_serialPort.write(qdata);
	if (sent == -1 || sent != qdata.size())
		throw EWriteError("No data could we written!");
}

void XpressNet::send(std::unique_ptr<const Cmd> cmd, UPCb ok, UPCb err, size_t no_sent) {
	log("PUT: " + cmd->msg(), LogLevel::Commands);

	try {
		m_lastSent = QDateTime::currentDateTime();
		send(cmd->getBytes());
		if (Xn::is<CmdAccOpRequest>(*cmd) && !this->liAcknowledgesSetAccState() &&
		    dynamic_cast<const CmdAccOpRequest &>(*cmd).state) {
			// acknowledge manually, do not add to pending buffer
			if (nullptr != ok)
				ok->func(this, ok->data);
		} else
			m_pending.emplace_back(cmd, timeout(cmd.get()), no_sent, std::move(ok), std::move(err));
	} catch (QStrException &) {
		log("Fatal error when writing command: " + cmd->msg(), LogLevel::Error);
		if (nullptr != err)
			err->func(this, err->data);
	}
}

void XpressNet::to_send(std::unique_ptr<const Cmd> &cmd, UPCb ok, UPCb err, size_t no_sent,
                        bool bypass_m_out_emptiness) {
	// Sends or queues
	if ((m_pending.size() >= _PENDING_MAX_AT_ONCE) || (!m_out.empty() && !bypass_m_out_emptiness) ||
	    conflictWithPending(*cmd)) {
		// Pending full -> push & do not start timer (response from CS will send the next command from m_out)
		// We ensure pending buffer never contains commands with conflict
		log("ENQUEUE: " + cmd->msg(), LogLevel::Debug);
		m_out.emplace_back(cmd, timeout(cmd.get()), no_sent, std::move(ok), std::move(err));
	} else {
		if (m_lastSent.addMSecs(m_config.outInterval) > QDateTime::currentDateTime()) {
			// Last command sent too early, still space in pending buffer ->
			// queue & activate timer for next send
			log("ENQUEUE: " + cmd->msg(), LogLevel::Debug);
			m_out.emplace_back(cmd, timeout(cmd.get()), no_sent, std::move(ok), std::move(err));
			if ((m_pending.empty()) && (!m_out_timer.isActive()))
				m_out_timer.start();
		} else {
			send(std::move(cmd), std::move(ok), std::move(err), no_sent);
		}
	}
}

void XpressNet::to_send(PendingItem &&pending, bool bypass_m_out_emptiness) {
	// Pending resending uses m_out queue (could try to resend multiple messages once)
	std::unique_ptr<const Cmd> cmd2(std::move(pending.cmd));
	to_send(cmd2, std::move(pending.callback_ok), std::move(pending.callback_err), pending.no_sent + 1,
	        bypass_m_out_emptiness);
}

void XpressNet::m_out_timer_tick() {
	if (m_out.empty()) {
		m_out_timer.stop();
	} else {
		if (m_pending.empty())
			send_next_out();
	}
}

void XpressNet::send_next_out() {
	if (m_lastSent.addMSecs(m_config.outInterval) > QDateTime::currentDateTime()) {
		if (!m_out_timer.isActive())
			m_out_timer.start();
		return;
	}

	PendingItem out = std::move(m_out.front());
	log("DEQUEUE: " + out.cmd->msg(), LogLevel::Debug);
	m_out.pop_front();
	to_send(std::move(out), true);
}

QDateTime XpressNet::timeout(const Cmd *x) {
	QDateTime timeout;
	if (Xn::is<CmdReadDirect>(*x) || Xn::is<CmdWriteDirect>(*x) ||
			Xn::is<CmdRequestReadResult>(*x) || Xn::is<CmdRequestWriteResult>(*x))
		timeout = QDateTime::currentDateTime().addMSecs(_PENDING_PROG_TIMEOUT);
	else
		timeout = QDateTime::currentDateTime().addMSecs(_PENDING_TIMEOUT);
	return timeout;
}

} // namespace Xn
