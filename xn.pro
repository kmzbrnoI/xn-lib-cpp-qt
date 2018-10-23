# QT += core
TARGET = xn
TEMPLATE = lib
DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += xn.cpp
HEADERS += xn.h xn-typedefs.h q-str-exception.h

CONFIG += c++14
QMAKE_CXXFLAGS += -Wall -Wextra -pedantic

QT += serialport

VERSION_MAJOR = 1
VERSION_MINOR = 0

DEFINES += "VERSION_MAJOR=$$VERSION_MAJOR" \
	"VERSION_MINOR=$$VERSION_MINOR"

#Target version
VERSION = $${VERSION_MAJOR}.$${VERSION_MINOR}