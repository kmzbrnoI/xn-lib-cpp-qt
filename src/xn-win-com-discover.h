#ifndef XNWINCOMDISCOVER_H
#define XNWINCOMDISCOVER_H

#include <QtGlobal>

#ifdef Q_OS_WIN

#include <QSerialPortInfo>
#include <vector>
#include <windows.h>

std::vector<QSerialPortInfo> winULIPorts();

#endif

#endif // XNWINCOMDISCOVER_H
