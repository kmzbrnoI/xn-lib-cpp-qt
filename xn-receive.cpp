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

		try {
			parseMessage(message);
		} catch (const QStrException& e) {
			log("parseMessage exception: "+e.str(), LogLevel::Error);
		} catch (...) {
			log("parseMessage general exception!", LogLevel::Error);
		}

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

		if (!m_pending.empty() && is<CmdReadDirect>(m_pending.front())) {
			const auto &rd = dynamic_cast<const CmdReadDirect &>(*(m_pending.front().cmd));
			to_send(CmdRequestReadResult(rd.cv, rd.callback), std::move(m_pending.front().callback_ok),
			        std::move(m_pending.front().callback_err));
		} else if (!m_pending.empty() && is<CmdWriteDirect>(m_pending.front())) {
			const auto &wr = dynamic_cast<const CmdWriteDirect &>(*(m_pending.front().cmd));
			to_send(CmdRequestWriteResult(wr.cv, wr.data), std::move(m_pending.front().callback_ok),
			        std::move(m_pending.front().callback_err));
		}
		if (!m_pending.empty() && m_pending.front().cmd->okResponse())
			pending_ok();
	} else if (0x05 == msg[1]) {
		log("GET: ERR: The Command Station is no longer providing the LI "
		    "a timeslot for communication",
		    LogLevel::Error);
		this->pendingClear();
	} else if (0x06 == msg[1]) {
		log("GET: ERR: Buffer overflow in the LI", LogLevel::Error);
	} else if (0x07 == msg[1]) {
		log("GET: INFO: The Command Station started addressing LI again", LogLevel::Info);
	} else if (0x08 == msg[1]) {
		log("GET: ERR: No commands can currently be sent to the Command Station", LogLevel::Error);
		if (!m_pending.empty())
			pending_err();
	} else if (0x09 == msg[1]) {
		log("GET: ERR: Error in the command parameters", LogLevel::Error);
	} else if (0x0A == msg[1]) {
		log("GET: ERR: Unknown error (Command Station did not provide the expected answer)",
		    LogLevel::Error);
	}
}

void XpressNet::handleMsgLiVersion(MsgType &msg) {
	const uint8_t hw = msg[1];
	const uint8_t sw = msg[2];

	log("GET: LI version; HW: " + XpressNet::liVersionToStr(hw) + ", SW: " + XpressNet::liVersionToStr(sw),
	    LogLevel::Commands);
	this->checkLiVersionDeprecated(hw, sw);

	if (!m_pending.empty() && is<CmdGetLIVersion>(m_pending.front())) {
		std::unique_ptr<const Cmd> cmd = std::move(m_pending.front().cmd);
		pending_ok();
		const auto &pending = dynamic_cast<const CmdGetLIVersion &>(*cmd);
		if (pending.callback != nullptr)
			pending.callback(this, hw, sw);
	} else if (!m_pending.empty() && is<CmdGetLIAddress>(m_pending.front())) {
		// Report NanoX error faster
		pending_err();
	}
}

void XpressNet::checkLiVersionDeprecated(uint8_t hw, uint8_t sw)
{
	(void)hw;

	const unsigned ULI_MIN_SW = 0x24;
	if ((this->m_liType == LIType::uLI) && (sw < ULI_MIN_SW))
		log("uLI SW version is deprecated, update at least to " + XpressNet::liVersionToStr(ULI_MIN_SW),
		    LogLevel::Warning);
}

void XpressNet::handleMsgCsGeneralEvent(MsgType &msg) {
	if (0x00 == msg[1]) {
		log("GET: Status Off", LogLevel::Commands);
		if (!m_pending.empty() && is<CmdOff>(m_pending.front()))
			pending_ok();
		if (m_trk_status != TrkStatus::Off) {
			m_trk_status = TrkStatus::Off;
			emit onTrkStatusChanged(m_trk_status);
		}
	} else if (0x01 == msg[1]) {
		log("GET: Status On", LogLevel::Commands);
		if (!m_pending.empty() && is<CmdOn>(m_pending.front()))
			pending_ok();
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

		if (!m_pending.empty()) {
			if (is<CmdRequestReadResult>(m_pending.front())) {
				std::unique_ptr<const Cmd> cmd = std::move(m_pending.front().cmd);
				pending_ok();
				const CmdRequestReadResult *cmdrrr = dynamic_cast<const CmdRequestReadResult *>(cmd.get());
				cmdrrr->callback(this, static_cast<ReadCVStatus>(msg[1]), cmdrrr->cv, 0);
			} else if (is<CmdReadDirect>(m_pending.front())) {
				std::unique_ptr<const Cmd> cmd = std::move(m_pending.front().cmd);
				pending_ok();
				const CmdReadDirect *cmdrd = dynamic_cast<const CmdReadDirect*>(cmd.get());
				cmdrd->callback(this, static_cast<ReadCVStatus>(msg[1]), cmdrd->cv, 0);
			} else if ((!ok) && ((is<CmdRequestWriteResult>(m_pending.front())) || (is<CmdWriteDirect>(m_pending.front())))) {
				// Error in writing is reported as pending_error
				pending_err(false);
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

	if (!m_pending.empty() && is<CmdGetCSStatus>(m_pending.front()))
		pending_ok();

	if (n != m_trk_status) {
		m_trk_status = n;
		emit onTrkStatusChanged(m_trk_status);
	}
}

void XpressNet::handleMsgCsVersion(MsgType &msg) {
	const unsigned major = msg[2] >> 4;
	const unsigned minor =  msg[2] & 0x0F;
	const uint8_t id = msg[3];

	log("GET: Command Station Version " + QString::number(major) + "." +
		QString::number(minor) + ", id " + QString::number(id), LogLevel::Commands);

	if (!m_pending.empty() && is<CmdGetCSVersion>(m_pending.front())) {
		std::unique_ptr<const Cmd> cmd = std::move(m_pending.front().cmd);
		pending_ok();
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

	if (m_pending.empty())
		return;

	if (is<CmdRequestReadResult>(m_pending.front())) {
		std::unique_ptr<const Cmd> cmd = std::move(m_pending.front().cmd);
		pending_ok();
		dynamic_cast<const CmdRequestReadResult *>(cmd.get())->callback(
			this, ReadCVStatus::Ok, cv, value
		);
	} else if (is<CmdReadDirect>(m_pending.front())) {
		std::unique_ptr<const Cmd> cmd = std::move(m_pending.front().cmd);
		const CmdReadDirect *cmdrd = dynamic_cast<const CmdReadDirect *>(cmd.get());
		if (cv == cmdrd->cv) {
			pending_ok();
			cmdrd->callback(this, ReadCVStatus::Ok, cv, value);
		}
	} else if (is<CmdRequestWriteResult>(m_pending.front())) {
		std::unique_ptr<const Cmd> cmd = std::move(m_pending.front().cmd);
		if (value == dynamic_cast<const CmdRequestWriteResult *>(cmd.get())->value) {
			pending_ok();
		} else {
			// Mismatch in written & read CV values is reported as pending_err
			log("GET: Received value "+QString::number(value)+" does not match programmed value!", LogLevel::Error);
			pending_err(false);
		}
	} else if (is<CmdWriteDirect>(m_pending.front())) {
		std::unique_ptr<const Cmd> cmd = std::move(m_pending.front().cmd);
		if (value == dynamic_cast<const CmdWriteDirect *>(cmd.get())->data)
			pending_ok();
		// else mismatch -> ask for CV value again (send CmdRequestWriteResult)
	}
}

void XpressNet::handleMsgLocoInfo(MsgType &msg) {
	log("GET: loco information", LogLevel::Commands);

	if (!m_pending.empty() && is<CmdGetLocoInfo>(m_pending.front())) {
		std::unique_ptr<const Cmd> cmd = std::move(m_pending.front().cmd);
		pending_ok();

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

		if (!m_pending.empty() && is<CmdGetLocoFunc1328>(m_pending.front())) {
			std::unique_ptr<const Cmd> cmd = std::move(m_pending.front().cmd);
			pending_ok();

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
	if (!m_pending.empty() && is<CmdGetLIAddress>(m_pending.front())) {
		std::unique_ptr<const Cmd> cmd = std::move(m_pending.front().cmd);
		pending_ok();
		if (dynamic_cast<const CmdGetLIAddress *>(cmd.get())->callback != nullptr) {
			dynamic_cast<const CmdGetLIAddress *>(cmd.get())->callback(this, msg[2]);
		}
	} else if (!m_pending.empty() && is<CmdSetLIAddress>(m_pending.front())) {
		pending_ok();
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
		if ((!m_pending.empty()) && (is<CmdAccInfoRequest>(m_pending.front())) &&
		    (dynamic_cast<const CmdAccInfoRequest *>(m_pending.front().cmd.get())->groupAddr == groupAddr) &&
		    (dynamic_cast<const CmdAccInfoRequest *>(m_pending.front().cmd.get())->nibble == nibble))
			pending_ok();

		// Some command stations (with internal output->input feedback enabled)
		// send Acc feedback directly after AccOpRequest. The LI does not receive any
		// normal inquiry, thus it does not send expected "OK" response.
		// -> Check for this situation & call pending_ok on pending CmdAccOpRequest
		if ((!m_pending.empty()) && (is<CmdAccOpRequest>(m_pending.front()))) {
			const auto& accOpCmd = dynamic_cast<const CmdAccOpRequest *>(m_pending.front().cmd.get());
			unsigned int port = 8*groupAddr + 4*nibble + (accOpCmd->portAddr & 0x03);
			bool bstate = ((state.all & (1 << (accOpCmd->portAddr & 0x03))) > 0);
			if ((accOpCmd->portAddr == port) && (accOpCmd->state == bstate))
				pending_ok();
		}

		emit onAccInputChanged(groupAddr, nibble, error, inputType, state);
	}
}

///////////////////////////////////////////////////////////////////////////////

} // namespace Xn
