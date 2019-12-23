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
			// acknowledge manually, do not add to history buffer
			if (nullptr != ok)
				ok->func(this, ok->data);
		} else
			m_hist.emplace_back(cmd, timeout(cmd.get()), no_sent, std::move(ok), std::move(err));
	} catch (QStrException &) {
		log("Fatal error when writing command: " + cmd->msg(), LogLevel::Error);
		if (nullptr != err)
			err->func(this, err->data);
	}
}

void XpressNet::to_send(std::unique_ptr<const Cmd> &cmd, UPCb ok, UPCb err, size_t no_sent) {
	// Sends or queues
	if (m_hist.size() >= _MAX_HIST_BUF_COUNT) {
		// History full -> push & do not start timer (response from CS will send automatically)
		m_out.push(HistoryItem(cmd, timeout(cmd.get()), no_sent, std::move(ok), std::move(err)));
	} else {
		if (m_lastSent.addMSecs(_OUT_TIMER_INTERVAL) > QDateTime::currentDateTime()) {
			// Last command sent too early, still space in hist buffer ->
			// queue & activate timer for next send
			m_out.push(HistoryItem(cmd, timeout(cmd.get()), no_sent, std::move(ok), std::move(err)));
			if (!m_out_timer.isActive())
				m_out_timer.start();
		} else {
			send(std::move(cmd), std::move(ok), std::move(err));
		}
	}
}

template <typename T>
void XpressNet::to_send(const T &&cmd, UPCb ok, UPCb err) {
	std::unique_ptr<const Cmd> cmd2(std::make_unique<const T>(cmd));
	to_send(cmd2, std::move(ok), std::move(err));
}

void XpressNet::to_send(HistoryItem &&hist) {
	// History resending uses m_out queue (could try to resend multiple messages once)
	std::unique_ptr<const Cmd> cmd2(std::move(hist.cmd));
	to_send(cmd2, std::move(hist.callback_ok), std::move(hist.callback_err), hist.no_sent + 1);
}

void XpressNet::m_out_timer_tick() {
	if (m_out.empty())
		m_out_timer.stop();
	send_next_out();
}

void XpressNet::send_next_out() {
	HistoryItem out = std::move(m_out.front());
	m_out.pop();
	to_send(std::move(out));
}

QDateTime XpressNet::timeout(const Cmd *x) {
	QDateTime timeout;
	if (Xn::is<CmdReadDirect>(*x) || Xn::is<CmdRequestReadResult>(*x))
		timeout = QDateTime::currentDateTime().addMSecs(_HIST_PROG_TIMEOUT);
	else
		timeout = QDateTime::currentDateTime().addMSecs(_HIST_TIMEOUT);
	return timeout;
}

} // namespace Xn
