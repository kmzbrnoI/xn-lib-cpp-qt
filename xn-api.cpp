#include "xn.h"

/* XpressNet class public methods implementation. */

namespace Xn {

void XpressNet::connect(const QString &portname, int32_t br, QSerialPort::FlowControl fc,
                        LIType liType) {
	log("Connecting to " + portname + " (br=" + QString::number(br) + ") ...", LogLevel::Info);

	m_serialPort.setBaudRate(br);
	m_serialPort.setFlowControl(fc);
	m_serialPort.setPortName(portname);
	m_liType = liType;

	if (!m_serialPort.open(QIODevice::ReadWrite))
		throw EOpenError(m_serialPort.errorString());

	m_hist_timer.start(_HIST_CHECK_INTERVAL);
	log("Connected", LogLevel::Info);
	onConnect();
}

void XpressNet::disconnect() {
	log("Disconnecting...", LogLevel::Info);
	m_hist_timer.stop();
	m_serialPort.close();
	onDisconnect();
}

bool XpressNet::connected() const { return m_serialPort.isOpen(); }
TrkStatus XpressNet::getTrkStatus() const { return m_trk_status; }

///////////////////////////////////////////////////////////////////////////////

void XpressNet::setTrkStatus(const TrkStatus status, UPCb ok, UPCb err) {
	if (status == TrkStatus::Off) {
		to_send(CmdOff(), std::move(ok), std::move(err));
	} else if (status == TrkStatus::On) {
		to_send(CmdOn(), std::move(ok), std::move(err));
	} else {
		throw EInvalidTrkStatus("This track status cannot be set!");
	}
}

void XpressNet::emergencyStop(const LocoAddr addr, UPCb ok, UPCb err) {
	to_send(CmdEmergencyStopLoco(addr), std::move(ok), std::move(err));
}

void XpressNet::emergencyStop(UPCb ok, UPCb err) {
	to_send(CmdEmergencyStop(), std::move(ok), std::move(err));
}

void XpressNet::getCommandStationVersion(GotCSVersion const &callback, UPCb err) {
	to_send(CmdGetCSVersion(callback), nullptr, std::move(err));
}

void XpressNet::getCommandStationStatus(UPCb ok, UPCb err) {
	to_send(CmdGetCSStatus(), std::move(ok), std::move(err));
}

void XpressNet::getLIVersion(GotLIVersion const &callback, UPCb err) {
	to_send(CmdGetLIVersion(callback), nullptr, std::move(err));
}

void XpressNet::getLIAddress(GotLIAddress const &callback, UPCb err) {
	to_send(CmdGetLIAddress(callback), nullptr, std::move(err));
}

void XpressNet::setLIAddress(uint8_t addr, UPCb ok, UPCb err) {
	to_send(CmdSetLIAddress(addr), std::move(ok), std::move(err));
}

void XpressNet::pomWriteCv(const LocoAddr addr, uint16_t cv, uint8_t value, UPCb ok, UPCb err) {
	to_send(CmdPomWriteCv(addr, cv, value), std::move(ok), std::move(err));
}

void XpressNet::pomWriteBit(const LocoAddr addr, uint16_t cv, uint8_t biti, bool value, UPCb ok,
                            UPCb err) {
	to_send(CmdPomWriteBit(addr, cv, biti, value), std::move(ok), std::move(err));
}

void XpressNet::setSpeed(const LocoAddr addr, uint8_t speed, Direction direction, UPCb ok,
                         UPCb err) {
	to_send(CmdSetSpeedDir(addr, speed, direction), std::move(ok), std::move(err));
}

void XpressNet::getLocoInfo(const LocoAddr addr, GotLocoInfo const &callback, UPCb err) {
	to_send(CmdGetLocoInfo(addr, callback), nullptr, std::move(err));
}

void XpressNet::getLocoFunc1328(LocoAddr addr, GotLocoFunc1328 callback, UPCb err) {
	to_send(CmdGetLocoFunc1328(addr, callback), nullptr, std::move(err));
}

void XpressNet::setFuncA(const LocoAddr addr, const FA fa, UPCb ok, UPCb err) {
	to_send(CmdSetFuncA(addr, fa), std::move(ok), std::move(err));
}

void XpressNet::setFuncB(const LocoAddr addr, const FB fb, const FSet range, UPCb ok, UPCb err) {
	to_send(CmdSetFuncB(addr, fb, range), std::move(ok), std::move(err));
}

void XpressNet::setFuncC(LocoAddr addr, FC fc, UPCb ok, UPCb err) {
	to_send(CmdSetFuncC(addr, fc), std::move(ok), std::move(err));
}

void XpressNet::setFuncD(LocoAddr addr, FD fd, UPCb ok, UPCb err) {
	to_send(CmdSetFuncD(addr, fd), std::move(ok), std::move(err));
}

void XpressNet::readCVdirect(const uint8_t cv, ReadCV const &callback, UPCb err) {
	to_send(CmdReadDirect(cv, callback), nullptr, std::move(err));
}

void XpressNet::accInfoRequest(const uint8_t groupAddr, const bool nibble, UPCb err) {
	to_send(CmdAccInfoRequest(groupAddr, nibble), nullptr, std::move(err));
}

void XpressNet::accOpRequest(const uint16_t portAddr, const bool state, UPCb ok, UPCb err) {
	to_send(CmdAccOpRequest(portAddr, state), std::move(ok), std::move(err));
}

///////////////////////////////////////////////////////////////////////////////

} // namespace Xn
