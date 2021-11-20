#include <QSerialPortInfo>
#include <QMessageBox>

#include "lib-main.h"

/* Implementations of GUI functions from lib-main.h */

namespace Xn {

void LibMain::guiInit() {
	QObject::connect(form.ui.cb_interface_type, SIGNAL(currentIndexChanged(int)), this,
					 SLOT(cb_interface_type_changed(int)));
	QObject::connect(form.ui.cb_serial_port, SIGNAL(currentIndexChanged(int)), this,
	                 SLOT(cb_connections_changed(int)));
	QObject::connect(form.ui.cb_serial_speed, SIGNAL(currentIndexChanged(int)), this,
	                 SLOT(cb_connections_changed(int)));
	QObject::connect(form.ui.cb_serial_flowcontrol, SIGNAL(currentIndexChanged(int)), this,
	                 SLOT(cb_connections_changed(int)));

	QObject::connect(form.ui.b_serial_refresh, SIGNAL(released()), this,
	                 SLOT(b_serial_refresh_handle()));
	QObject::connect(form.ui.b_info_update, SIGNAL(released()), this,
	                 SLOT(b_info_update_handle()));
	QObject::connect(form.ui.b_li_addr_set, SIGNAL(released()), this,
	                 SLOT(b_li_addr_set_handle()));

	this->fillConnectionsCbs();

	QString text;
	text.asprintf("Nastavení XpressNET knihovny v%d.%d", VERSION_MAJOR, VERSION_MINOR);
	form.setWindowTitle(text);
	form.setFixedSize(form.size());
}

void LibMain::cb_connections_changed(int) {
	if (this->gui_config_changing)
		return;

	s["XN"]["interface"] = form.ui.cb_interface_type->currentText();
	s["XN"]["port"] = form.ui.cb_serial_port->currentText();
	s["XN"]["baudrate"] = form.ui.cb_serial_speed->currentText().toInt();
	s["XN"]["flowcontrol"] = form.ui.cb_serial_flowcontrol->currentIndex();
}

void LibMain::fillConnectionsCbs() {
	this->gui_config_changing = true;

	// Interface type
	form.ui.cb_interface_type->setCurrentText(s["XN"]["interface"].toString());

	// Port
	this->fillPortCb();
	this->gui_config_changing = true;

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

	this->gui_config_changing = false;
}

void LibMain::fillPortCb() {
	this->gui_config_changing = true;

	form.ui.cb_serial_port->clear();
	bool is_item = false;
    const auto& ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &port : ports) {
		form.ui.cb_serial_port->addItem(port.portName());
		if (port.portName() == s["XN"]["port"].toString())
			is_item = true;
	}
	if (is_item)
		form.ui.cb_serial_port->setCurrentText(s["XN"]["port"].toString());
	else
		form.ui.cb_serial_port->setCurrentIndex(-1);

	this->gui_config_changing = false;
}

void LibMain::b_serial_refresh_handle() { this->fillPortCb(); }

void LibMain::guiOnOpen() {
	form.ui.cb_interface_type->setEnabled(false);
	form.ui.cb_serial_port->setEnabled(false);
	form.ui.cb_serial_speed->setEnabled(false);
	form.ui.cb_serial_flowcontrol->setEnabled(false);
	form.ui.b_serial_refresh->setEnabled(false);

	form.ui.sb_li_addr->setEnabled(true);
	form.ui.b_li_addr_set->setEnabled(true);
	form.ui.b_info_update->setEnabled(true);
}

void LibMain::guiOnClose() {
	form.ui.cb_interface_type->setEnabled(true);
	form.ui.cb_serial_port->setEnabled(true);
	form.ui.cb_serial_speed->setEnabled(true);
	form.ui.cb_serial_flowcontrol->setEnabled(true);
	form.ui.b_serial_refresh->setEnabled(true);

	form.ui.l_cs_version->setText("???");
	form.ui.l_cs_id->setText("???");
	form.ui.l_li_version->setText("???");
	form.ui.sb_li_addr->setEnabled(false);
	form.ui.sb_li_addr->setValue(0);
	form.ui.b_li_addr_set->setEnabled(false);
	form.ui.b_info_update->setEnabled(false);
	form.ui.l_info_datetime->setText("???");
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
