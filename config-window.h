#ifndef CONFIG_WINDOW_H
#define CONFIG_WINDOW_H

#include <QMainWindow>

#include "ui_config-window.h"

namespace Xn {

class ConfigWindow : public QMainWindow {
	Q_OBJECT
public:
	Ui::MainWindow ui;
	ConfigWindow(QWidget *parent = nullptr) : QMainWindow(parent) { ui.setupUi(this); }
};

} // namespace Xn

#endif
