TARGET = xn
TEMPLATE = lib
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += XN_SHARED_LIBRARY

SOURCES += \
	src/li/xn-li-com.cpp \
	src/li/xn-li-net.cpp \
	src/xn.cpp \
	src/xn-api.cpp \
	src/xn-receive.cpp \
	src/xn-send.cpp \
	src/xn-pending.cpp \
	src/xn-win-com-discover.cpp \
	src/li/xn-li.cpp
HEADERS += \
	src/li/xn-li-com.h \
	src/li/xn-li-net.h \
	src/xn.h \
	src/xn-loco-addr.h \
	src/xn-commands.h \
	src/q-str-exception.h \
	src/xn-win-com-discover.h \
	src/li/xn-li.h

# Do not import when using as static library
SOURCES += \
	src/lib-api.cpp \
	src/lib-main.cpp \
	src/settings.cpp \
	src/config-window.cpp
HEADERS += \
	src/lib-api.h \
	src/lib-main.h \
	src/lib-events.h \
	src/lib-api-common-def.h \
	src/settings.h \
	src/lib-errors.h

FORMS += ui/config-window.ui

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
VERSION_MINOR = 9

DEFINES += "VERSION_MAJOR=$$VERSION_MAJOR" \
	"VERSION_MINOR=$$VERSION_MINOR"

#Target version
VERSION = $${VERSION_MAJOR}.$${VERSION_MINOR}
DEFINES += "VERSION=\\\"$${VERSION}\\\""
