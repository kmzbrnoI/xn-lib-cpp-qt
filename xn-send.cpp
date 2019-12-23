#include <sstream>

/* Sending data to XpressNet. */

#include "xn.h"

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

void XpressNet::to_send(std::unique_ptr<const Cmd> &cmd, UPCb ok, UPCb err) {
	// Sends or queues
	if (m_hist.size() >= _MAX_HIST_BUF_COUNT) {
		m_out.push(HistoryItem(cmd, timeout(cmd.get()), 1, std::move(ok), std::move(err)));
	} else {
		send(std::move(cmd), std::move(ok), std::move(err));
	}
}

void XpressNet::send(std::unique_ptr<const Cmd> cmd, UPCb ok, UPCb err) {
	log("PUT: " + cmd->msg(), LogLevel::Commands);

	try {
		send(cmd->getBytes());
		if (Xn::is<CmdAccOpRequest>(*cmd) && !this->liAcknowledgesSetAccState() &&
			dynamic_cast<const CmdAccOpRequest &>(*cmd).state) {
			// acknowledge manually, do not add to history buffer
			if (nullptr != ok)
				ok->func(this, ok->data);
		} else
			m_hist.emplace_back(cmd, timeout(cmd.get()), 1, std::move(ok), std::move(err));
	} catch (QStrException &) {
		log("Fatal error when writing command: " + cmd->msg(), LogLevel::Error);
		if (nullptr != err)
			err->func(this, err->data);
	}
}

template <typename T>
void XpressNet::to_send(const T &&cmd, UPCb ok, UPCb err) {
	std::unique_ptr<const Cmd> cmd2(std::make_unique<const T>(cmd));
	to_send(cmd2, std::move(ok), std::move(err));
}

void XpressNet::send(HistoryItem &&hist) {
	hist.no_sent++;
	hist.timeout = timeout(hist.cmd.get());

	log("PUT: " + hist.cmd->msg(), LogLevel::Commands);

	try {
		send(hist.cmd->getBytes());
		m_hist.push_back(std::move(hist));
	} catch (QStrException &e) {
		log("PUT ERR: " + e, LogLevel::Error);
		throw;
	}
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
