TARGET = xn
TEMPLATE = lib
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += XN_SHARED_LIBRARY

SOURCES += \
	xn.cpp \
	xn-api.cpp \
	xn-receive.cpp \
	xn-send.cpp \
	xn-pending.cpp \
	xn-win-com-discover.cpp
HEADERS += \
	xn.h \
	xn-loco-addr.h \
	xn-commands.h \
	q-str-exception.h \
	xn-win-com-discover.h

# Do not import when using as static library
SOURCES += \
	lib-api.cpp \
	lib-main.cpp \
	settings.cpp \
	config-window.cpp
HEADERS += \
	lib-api.h \
	lib-main.h \
	lib-events.h \
	lib-api-common-def.h \
	settings.h \
	lib-errors.h

FORMS += config-window.ui

CONFIG += c++14 dll
QMAKE_CXXFLAGS += -Wall -Wextra -pedantic

win32 {
	QMAKE_LFLAGS += -Wl,--kill-at
	QMAKE_CXXFLAGS += -enable-stdcall-fixup
	LIBS += -lsetupapi
}
win64 {
	QMAKE_LFLAGS += -Wl,--kill-at
	QMAKE_CXXFLAGS += -enable-stdcall-fixup
	LIBS += -lsetupapi
}

QT += core gui serialport
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

VERSION_MAJOR = 2
VERSION_MINOR = 7

DEFINES += "VERSION_MAJOR=$$VERSION_MAJOR" \
	"VERSION_MINOR=$$VERSION_MINOR"

#Target version
VERSION = $${VERSION_MAJOR}.$${VERSION_MINOR}
DEFINES += "VERSION=\\\"$${VERSION}\\\""
