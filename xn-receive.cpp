#include "xn.h"

/* Receiving data from XpressNet & parsing it. */

namespace Xn {

void XpressNet::handleReadyRead() {
	// check timeout
	if (m_receiveTimeout < QDateTime::currentDateTime() && m_readData.size() > 0) {
		// clear input buffer when data not received for a long time
		m_readData.clear();
	}

	m_readData.append(m_serialPort.readAll());
	m_receiveTimeout = QDateTime::currentDateTime().addMSecs(_BUF_IN_TIMEOUT);

	if (this->m_liType == LIType::LIUSBEth) {
		// remove message till 0xFF 0xFE or 0xFF 0xFD
		int fe_pos = m_readData.indexOf(QByteArray("\xFF\xFE"));
		int fd_pos = m_readData.indexOf(QByteArray("\xFF\xFD"));
		if (fe_pos == -1)
			fe_pos = m_readData.size();
		if (fd_pos == -1)
			fd_pos = m_readData.size();
		m_readData.remove(0, std::min(fe_pos, fd_pos));
	}

	int length_pos = (this->m_liType == LIType::LIUSBEth ? 2 : 0);

	while (m_readData.size() > length_pos &&
	       m_readData.size() >= (m_readData[length_pos] & 0x0F)+2+length_pos) {
		unsigned int length = (m_readData[length_pos] & 0x0F)+2; // including header byte & xor
		uint8_t x = 0;
		for (unsigned int i = length_pos; i < length_pos+length; i++)
			x ^= m_readData[i];

		log("GET: " + dataToStr<QByteArray, uint8_t>(m_readData, length_pos+length),
		    LogLevel::RawData);

		if (x != 0) {
			// XOR error
			log("XOR error: " + dataToStr<QByteArray, uint8_t>(m_readData, length_pos+length),
			    LogLevel::Warning);
			m_readData.remove(0, static_cast<int>(length_pos+length));
			continue;
		}

		auto begin = m_readData.begin();
		for (int i = 0; i < length_pos; i++)
			++begin;

		std::vector<uint8_t> message(begin, begin + length);
		parseMessage(message);
		m_readData.remove(0, static_cast<int>(length_pos+length));
	}
}

void XpressNet::parseMessage(MsgType &msg) {
	switch (static_cast<RecvCmdType>(msg[0])) {
	case RecvCmdType::LiError:
		return handleMsgLiError(msg);
	case RecvCmdType::LiVersion:
		return handleMsgLiVersion(msg);
	case RecvCmdType::LiSettings:
		if (0x01 == msg[1])
			return handleMsgLIAddr(msg);
		else
			return;
	case RecvCmdType::CsGeneralEvent:
		return handleMsgCsGeneralEvent(msg);
	case RecvCmdType::CsStatus:
		if (0x22 == msg[1])
			return handleMsgCsStatus(msg);
		else
			return;
	case RecvCmdType::CsX63:
		if (0x21 == msg[1])
			return handleMsgCsVersion(msg);
		else if (0x14 == msg[1])
			return handleMsgCvRead(msg);
		else
			return;
	case RecvCmdType::CsLocoInfo:
		return handleMsgLocoInfo(msg);
	case RecvCmdType::CsLocoFunc:
		return handleMsgLocoFunc(msg);
	case RecvCmdType::CsAccInfoResp:
	case RecvCmdType::CsFeedbackBroadcast:
		break; // intentionally nothing, see if below
	}

	if ((msg[0] & 0xF0) == static_cast<uint8_t>(RecvCmdType::CsFeedbackBroadcast))
		return handleMsgAcc(msg);
}

void XpressNet::handleMsgLiError(MsgType &msg) {
	if (0x01 == msg[1]) {
		log("GET: Error occurred between the interfaces and the PC", LogLevel::Error);
	} else if (0x02 == msg[1]) {
		log("GET: Error occurred between the interfaces and the command station", LogLevel::Error);
	} else if (0x03 == msg[1]) {
		log("GET: Unknown communication error", LogLevel::Error);
	} else if (0x04 == msg[1]) {
		log("GET: OK", LogLevel::Commands);

		if (!m_hist.empty() && is<CmdReadDirect>(m_hist.front())) {
			const auto &rd = dynamic_cast<const CmdReadDirect &>(*(m_hist.front().cmd));
			to_send(CmdRequestReadResult(rd.cv, rd.callback), std::move(m_hist.front().callback_ok),
			        std::move(m_hist.front().callback_err));
		} else if (!m_hist.empty() && is<CmdWriteDirect>(m_hist.front())) {
			const auto &wr = dynamic_cast<const CmdWriteDirect &>(*(m_hist.front().cmd));
			to_send(CmdRequestWriteResult(wr.cv, wr.data), std::move(m_hist.front().callback_ok),
			        std::move(m_hist.front().callback_err));
		}
		if (!m_hist.empty() && m_hist.front().cmd->okResponse())
			hist_ok();
	} else if (0x05 == msg[1]) {
		log("GET: ERR: The Command Station is no longer providing the LI "
		    "a timeslot for communication",
		    LogLevel::Error);
		this->histClear();
	} else if (0x06 == msg[1]) {
		log("GET: ERR: Buffer overflow in the LI", LogLevel::Error);
	} else if (0x07 == msg[1]) {
		log("GET: INFO: The Command Station started addressing LI again", LogLevel::Info);
	} else if (0x08 == msg[1]) {
		log("GET: ERR: No commands can currently be sent to the Command Station", LogLevel::Error);
		if (!m_hist.empty())
			hist_err();
	} else if (0x09 == msg[1]) {
		log("GET: ERR: Error in the command parameters", LogLevel::Error);
	} else if (0x0A == msg[1]) {
		log("GET: ERR: Unknown error (Command Station did not provide the expected answer)",
		    LogLevel::Error);
	}
}

void XpressNet::handleMsgLiVersion(MsgType &msg) {
	unsigned hw = (msg[1] & 0x0F) + 10*(msg[1] >> 4);
	unsigned sw = (msg[2] & 0x0F) + 10*(msg[2] >> 4);

	log("GET: LI version; HW: " + QString::number(hw) + ", SW: " + QString::number(sw),
	    LogLevel::Commands);

	if (!m_hist.empty() && is<CmdGetLIVersion>(m_hist.front())) {
		std::unique_ptr<const Cmd> cmd = std::move(m_hist.front().cmd);
		hist_ok();
		const auto &hist = dynamic_cast<const CmdGetLIVersion &>(*cmd);
		if (hist.callback != nullptr)
			hist.callback(this, hw, sw);
	} else if (!m_hist.empty() && is<CmdGetLIAddress>(m_hist.front())) {
		// Report NanoX error faster
		hist_err();
	}
}

void XpressNet::handleMsgCsGeneralEvent(MsgType &msg) {
	if (0x00 == msg[1]) {
		log("GET: Status Off", LogLevel::Commands);
		if (!m_hist.empty() && is<CmdOff>(m_hist.front()))
			hist_ok();
		if (m_trk_status != TrkStatus::Off) {
			m_trk_status = TrkStatus::Off;
			emit onTrkStatusChanged(m_trk_status);
		}
	} else if (0x01 == msg[1]) {
		log("GET: Status On", LogLevel::Commands);
		if (!m_hist.empty() && is<CmdOn>(m_hist.front()))
			hist_ok();
		if (m_trk_status != TrkStatus::On) {
			m_trk_status = TrkStatus::On;
			emit onTrkStatusChanged(m_trk_status);
		}
	} else if (0x02 == msg[1]) {
		log("GET: Status Programming", LogLevel::Commands);
		if (m_trk_status != TrkStatus::Programming) {
			m_trk_status = TrkStatus::Programming;
			emit onTrkStatusChanged(m_trk_status);
		}
	} else if (0x11 == msg[1] || 0x12 == msg[1] || 0x13 == msg[1] || 0x1F == msg[1]) {
		bool ok = (msg[1] == 0x11);
		const QString message = xnReadCVStatusToQString(static_cast<ReadCVStatus>(msg[1]));
		log("GET: Programming info: "+message, ok ? LogLevel::Info : LogLevel::Error);

		if (!m_hist.empty()) {
			if (is<CmdRequestReadResult>(m_hist.front())) {
				std::unique_ptr<const Cmd> cmd = std::move(m_hist.front().cmd);
				hist_ok();
				const CmdRequestReadResult *cmdrrr = dynamic_cast<const CmdRequestReadResult *>(cmd.get());
				cmdrrr->callback(this, static_cast<ReadCVStatus>(msg[1]), cmdrrr->cv, 0);
			} else if (is<CmdReadDirect>(m_hist.front())) {
				std::unique_ptr<const Cmd> cmd = std::move(m_hist.front().cmd);
				hist_ok();
				const CmdReadDirect *cmdrd = dynamic_cast<const CmdReadDirect*>(cmd.get());
				cmdrd->callback(this, static_cast<ReadCVStatus>(msg[1]), cmdrd->cv, 0);
			} else if ((!ok) && ((is<CmdRequestWriteResult>(m_hist.front())) || (is<CmdWriteDirect>(m_hist.front())))) {
				// Error in writing is reported as hist_error
				hist_err(false);
			}
		}
	} else if (0x80 == msg[1]) {
		log("GET: command station reported transfer errors", LogLevel::Error);
	} else if (0x81 == msg[1]) {
		log("GET: command station busy", LogLevel::Error);
	} else if (0x82 == msg[1]) {
		log("GET: instruction not supported by command station", LogLevel::Error);
	}
}

void XpressNet::handleMsgCsStatus(MsgType &msg) {
	log("GET: command station status", LogLevel::Commands);
	TrkStatus n;
	if (msg[2] & 0x03)
		n = TrkStatus::Off;
	else if ((msg[2] >> 3) & 0x01)
		n = TrkStatus::Programming;
	else
		n = TrkStatus::On;

	if (!m_hist.empty() && is<CmdGetCSStatus>(m_hist.front()))
		hist_ok();

	if (n != m_trk_status) {
		m_trk_status = n;
		emit onTrkStatusChanged(m_trk_status);
	}
}

void XpressNet::handleMsgCsVersion(MsgType &msg) {
	unsigned major = msg[2] >> 4;
	unsigned minor =  msg[2] & 0x0F;
	uint8_t id = msg[3];

	log("GET: Command Station Version " + QString::number(major) + "." +
		QString::number(minor) + ", id " + QString::number(id), LogLevel::Commands);

	if (!m_hist.empty() && is<CmdGetCSVersion>(m_hist.front())) {
		std::unique_ptr<const Cmd> cmd = std::move(m_hist.front().cmd);
		hist_ok();
		if (dynamic_cast<const CmdGetCSVersion *>(cmd.get())->callback != nullptr) {
			dynamic_cast<const CmdGetCSVersion *>(cmd.get())->callback(
				this, major, minor, id
			);
		}
	}
}

void XpressNet::handleMsgCvRead(MsgType &msg) {
	uint8_t cv = msg[2];
	uint8_t value = msg[3];

	log("GET: CV " + QString::number(cv) + " value=" + QString::number(value),
	    LogLevel::Commands);

	if (m_hist.empty())
		return;

	if (is<CmdRequestReadResult>(m_hist.front())) {
		std::unique_ptr<const Cmd> cmd = std::move(m_hist.front().cmd);
		hist_ok();
		dynamic_cast<const CmdRequestReadResult *>(cmd.get())->callback(
			this, ReadCVStatus::Ok, cv, value
		);
	} else if (is<CmdReadDirect>(m_hist.front())) {
		std::unique_ptr<const Cmd> cmd = std::move(m_hist.front().cmd);
		const CmdReadDirect *cmdrd = dynamic_cast<const CmdReadDirect *>(cmd.get());
		if (cv == cmdrd->cv) {
			hist_ok();
			cmdrd->callback(this, ReadCVStatus::Ok, cv, value);
		}
	} else if (is<CmdRequestWriteResult>(m_hist.front())) {
		std::unique_ptr<const Cmd> cmd = std::move(m_hist.front().cmd);
		if (value == dynamic_cast<const CmdRequestWriteResult *>(cmd.get())->value) {
			hist_ok();
		} else {
			// Mismatch in written & read CV values is reported as hist_err
			log("GET: Received value "+QString::number(value)+" does not match programmed value!", LogLevel::Error);
			hist_err(false);
		}
	} else if (is<CmdWriteDirect>(m_hist.front())) {
		std::unique_ptr<const Cmd> cmd = std::move(m_hist.front().cmd);
		if (value == dynamic_cast<const CmdWriteDirect *>(cmd.get())->data)
			hist_ok();
		// else mismatch -> ask for CV value again (send CmdRequestWriteResult)
	}
}

void XpressNet::handleMsgLocoInfo(MsgType &msg) {
	log("GET: loco information", LogLevel::Commands);

	if (!m_hist.empty() && is<CmdGetLocoInfo>(m_hist.front())) {
		std::unique_ptr<const Cmd> cmd = std::move(m_hist.front().cmd);
		hist_ok();

		bool used = (msg[1] >> 3) & 0x01;
		unsigned mode = msg[1] & 0x07;
		auto direction = static_cast<Direction>(msg[2] >> 7);
		unsigned speed;

		// Normalize speed to 28 speed steps
		if (mode == 0) {
			// 14 speed steps
			speed = msg[2] & 0xF;
			if (speed > 0)
				speed -= 1;
			speed *= 2;
		} else if (mode == 1) {
			// 27 speed steps
			speed = ((msg[2] & 0xF) << 1) + ((msg[2] >> 4) & 0x1);
			if (speed < 4)
				speed = 0;
			else
				speed -= 3;
			speed = speed * (28./27);
		} else if (mode == 2) {
			// 28 speed steps
			speed = ((msg[2] & 0xF) << 1) + ((msg[2] >> 4) & 0x1);
			if (speed < 4)
				speed = 0;
			else
				speed -= 3;
		} else {
			// 128 speed steps
			speed = msg[2] & 0x7F;
			if (speed > 0)
				speed -= 1;
			speed = speed * (28./128);
		}

		if (dynamic_cast<const CmdGetLocoInfo *>(cmd.get())->callback != nullptr) {
			dynamic_cast<const CmdGetLocoInfo *>(cmd.get())->callback(
			    this, used, direction, speed, FA(msg[3]), FB(msg[4]));
		}
	}
}

void XpressNet::handleMsgLocoFunc(MsgType &msg) {
	if (msg[1] == 0x40) {
		try {
			LocoAddr addr(msg[3], msg[2]);
			log("GET: Loco "+QString(addr)+" stolen", LogLevel::Commands);
			emit this->onLocoStolen(addr);
		} catch (...) {

		}
	} else if (msg[1] == 0x52) {
		log("GET: Loco Func 13-28 Status", LogLevel::Commands);

		if (!m_hist.empty() && is<CmdGetLocoFunc1328>(m_hist.front())) {
			std::unique_ptr<const Cmd> cmd = std::move(m_hist.front().cmd);
			hist_ok();

			if (dynamic_cast<const CmdGetLocoFunc1328 *>(cmd.get())->callback != nullptr) {
				dynamic_cast<const CmdGetLocoFunc1328 *>(cmd.get())->callback(
					this, FC(msg[2]), FD(msg[3])
				);
			}
		}
	}
}

void XpressNet::handleMsgLIAddr(MsgType &msg) {
	log("GET: LI Address is " + QString::number(msg[2]), LogLevel::Commands);
	if (!m_hist.empty() && is<CmdGetLIAddress>(m_hist.front())) {
		std::unique_ptr<const Cmd> cmd = std::move(m_hist.front().cmd);
		hist_ok();
		if (dynamic_cast<const CmdGetLIAddress *>(cmd.get())->callback != nullptr) {
			dynamic_cast<const CmdGetLIAddress *>(cmd.get())->callback(this, msg[2]);
		}
	} else if (!m_hist.empty() && is<CmdSetLIAddress>(m_hist.front())) {
		hist_ok();
	}
}

void XpressNet::handleMsgAcc(MsgType &msg) {
	const uint8_t bytes = (msg[0] & 0x0F);
	if ((bytes%2) != 0) {
		log("GET: Invalid Feedback Broadcast length (not even), ignoring packet!", LogLevel::Warning);
		return;
	}
	if (msg.size() != (bytes+2U)) {
		log("GET: Invalid Feedback Broadcast vector length, ignoring packet!", LogLevel::Warning);
		return;
	}

	for (unsigned i = 0; i < bytes; i += 2) {
		uint8_t groupAddr = msg[1+i];
		bool nibble = (msg[2+i] >> 4) & 0x1;
		bool error = msg[2+i] >> 7;
		auto inputType = static_cast<FeedbackType>((msg[2+i] >> 5) & 0x3);
		AccInputsState state;
		state.all = msg[2+i] & 0x0F;

		log("GET: Acc state: group " + QString::number(groupAddr) + ", nibble " +
		    QString::number(nibble) + ", state " + QString::number(state.all, 2).rightJustified(4, '0'),
		    LogLevel::Commands);
		if ((!m_hist.empty()) && (is<CmdAccInfoRequest>(m_hist.front())) &&
		    (dynamic_cast<const CmdAccInfoRequest *>(m_hist.front().cmd.get())->groupAddr == groupAddr) &&
		    (dynamic_cast<const CmdAccInfoRequest *>(m_hist.front().cmd.get())->nibble == nibble))
			hist_ok();

		emit onAccInputChanged(groupAddr, nibble, error, inputType, state);
	}
}

///////////////////////////////////////////////////////////////////////////////

} // namespace Xn
