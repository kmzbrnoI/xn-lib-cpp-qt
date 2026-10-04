#include <QSerialPortInfo>
#include <QMessageBox>

#include "lib-main.h"

/* Implementations of GUI functions from lib-main.h */

namespace Xn {

void LibMain::guiInit() {
	QObject::connect(form.ui.cb_interface_type, SIGNAL(currentIndexChanged(int)), this,
	                 SLOT(cb_interface_type_changed(int)));

	QObject::connect(form.ui.b_serial_refresh, SIGNAL(released()), this,
	                 SLOT(b_serial_refresh_handle()));
	QObject::connect(form.ui.b_info_update, SIGNAL(released()), this,
	                 SLOT(b_info_update_handle()));
	QObject::connect(form.ui.b_li_addr_set, SIGNAL(released()), this,
	                 SLOT(b_li_addr_set_handle()));

	this->fillConnectionsCbs();

	form.setWindowFlags(Qt::Dialog);
	form.setWindowTitle(QString::asprintf("Nastavení XpressNET knihovny v%d.%d", VERSION_MAJOR, VERSION_MINOR));
}

void LibMain::cb_interface_type_changed(int) {
	const bool net = form.ui.cb_interface_type->currentText().endsWith("net");
	form.ui.tw_connection->setTabVisible(0, !net);
	form.ui.tw_connection->setTabVisible(1, net);

	const bool uLI = (form.ui.cb_interface_type->currentText() == "uLI");
	if (uLI) {
		form.ui.cb_serial_speed->setCurrentText("19200");
		form.ui.cb_serial_flowcontrol->setCurrentIndex(0);
	}
	form.ui.cb_serial_speed->setEnabled(!uLI);
	form.ui.cb_serial_flowcontrol->setEnabled(!uLI);

	if ((s["XN"]["port"].toString() == "auto") && (form.ui.cb_interface_type->currentText() != "uLI"))
		s["XN"]["port"] = "";
	this->fillPortCb();
}

void LibMain::applyConnectionInfoFromGUI() {
	s["XN"]["interface"] = form.ui.cb_interface_type->currentText();
	s["XN"]["baudrate"] = form.ui.cb_serial_speed->currentText().toInt();
	s["XN"]["flowcontrol"] = form.ui.cb_serial_flowcontrol->currentIndex();
	s["XN"]["netHost"] = form.ui.le_host->text();
	s["XN"]["netPort"] = form.ui.sb_port->value();

	const QString port = form.ui.cb_serial_port->currentText();
	s["XN"]["port"] = (port.startsWith("Auto")) ? "auto" : port;
}

void LibMain::fillConnectionsCbs() {
	// Interface type
	form.ui.cb_interface_type->setCurrentText(s["XN"]["interface"].toString());

	// Port
	this->fillPortCb();

	// Speed
	form.ui.cb_serial_speed->clear();
	bool is_item = false;
	const auto& baudRates = QSerialPortInfo::standardBaudRates();
	for (const qint32 &br : baudRates) {
		form.ui.cb_serial_speed->addItem(QString::number(br));
		if (br == s["XN"]["baudrate"].toInt())
			is_item = true;
	}
	if (is_item)
		form.ui.cb_serial_speed->setCurrentText(s["XN"]["baudrate"].toString());
	else
		form.ui.cb_serial_speed->setCurrentIndex(-1);

	// Flow control
	form.ui.cb_serial_flowcontrol->setCurrentIndex(s["XN"]["flowcontrol"].toInt());

	form.ui.le_host->setText(s["XN"]["netHost"].toString());
	form.ui.sb_port->setValue(s["XN"]["netPort"].toInt());
}

void LibMain::fillPortCb() {
	form.ui.cb_serial_port->clear();

	bool is_item = false;

	if (form.ui.cb_interface_type->currentText() == "uLI") {
		form.ui.cb_serial_port->addItem("Automaticky detekovat port uLI");
		if (s["XN"]["port"].toString() == "auto") {
			is_item = true;
			form.ui.cb_serial_port->setCurrentIndex(0);
		}
	}

	const auto& ports = QSerialPortInfo::availablePorts();
	for (const QSerialPortInfo &port : ports) {
		form.ui.cb_serial_port->addItem(port.portName());
		if (port.portName() == s["XN"]["port"].toString())
			is_item = true;
	}

	if (s["XN"]["port"].toString() != "auto") {
		if (is_item)
			form.ui.cb_serial_port->setCurrentText(s["XN"]["port"].toString());
		else
			form.ui.cb_serial_port->setCurrentIndex(-1);
	}
}

void LibMain::b_serial_refresh_handle() { this->fillPortCb(); }

void LibMain::guiOnOpening() {
	form.ui.gb_connection->setEnabled(false);
}

void LibMain::guiOnOpen() {
	this->guiOnOpening();
	form.ui.gb_cs_li->setEnabled(true);
}

void LibMain::guiOnClose() {
	form.ui.gb_connection->setEnabled(true);
	form.ui.gb_cs_li->setEnabled(false);
}

void LibMain::b_info_update_handle() {
	form.ui.l_cs_version->setText("???");
	form.ui.l_cs_id->setText("???");
	form.ui.l_li_version->setText("???");
	form.ui.sb_li_addr->setValue(0);
	this->getLIVersion();
}

void LibMain::userLiAddrSet() {
	QMessageBox::information(&form, "Info", "Adresa LI úspěšně změněna.", QMessageBox::Ok);
}

void LibMain::userLiAddrSetErr() {
	QMessageBox::warning(&form, "Chyba", "Nepodařilo se změnit adresu LI!", QMessageBox::Ok);
}

void LibMain::b_li_addr_set_handle() {
	int addr = form.ui.sb_li_addr->value();
	QMessageBox::StandardButton reply = QMessageBox::question(
		&form, "Smazat?", "Skutečné změnit adresu LI na "+QString::number(addr)+"?",
		QMessageBox::Yes | QMessageBox::No
	);
	if (reply != QMessageBox::Yes)
		return;

	try {
		xn.setLIAddress(
			addr,
			std::make_unique<Cb>([this](void *, void *) { userLiAddrSet(); }),
			std::make_unique<Cb>([this](void *, void *) { userLiAddrSetErr(); })
		);
	} catch (const QStrException &e) {
		userLiAddrSetErr();
	}
}

} // namespace Xn
