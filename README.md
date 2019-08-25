# C++ Qt XpressNET library

This repository contains a fully open-source C++ library for communicating
with [XpressNET](https://dccwiki.com/XpressNet_Protocol)
[DCC Command Station](https://dccwiki.com/Command_station).

It connects to the (virtual) serial port, which is typically connected to the
LI.

Currently, it implements commands of [XpressNET
v3.0](http://www.lenzusa.com/1newsite1/Manuals/xpressnet.pdf) protocol,
however, you may simply add more commands from newer versions of XpressNET (for
example [XpressNET
v3.6](https://www.lenz-elektronik.de/pdf/XpressNet%20und%20USB%20Interface.pdf)).

## Requirements for use

This library uses [Qt](https://www.qt.io/)'s
[SerialPort](http://doc.qt.io/qt-5/qtserialport-index.html) which creates a
very good cross-platform abstraction of serial port interface. Thus, the
library uses Qt's mechanisms like slots and signals.

It is **not** usable without Qt.

There are no other requirements.

## Usage

You may use this library in two major ways:

 * Simply include `xn.h` header file into your project and use instance of
   `XpressNet` class.
 * Compile this project using `qmake` and use compiled object file.

## Basic information

 * See `xn.h` for API specification.
 * This library uses 28 speed steps only. It simplifies things a lot. Other
   speed steps could be added in future.
 * To change the version of this library, update both constants at `xn.pro`
   file and `xn.h` file. This is needed for proper behavior as a standalone-lib
   and plain header too.

## Building & toolkit

This SW was developed in `vim` using `qmake` & `make`. Downloads are available
in *Releases* section. It is suggested to use `clang` as a compiler, because
then you may use `clang-tools` during development process (see below).

### Prerequisities

 * Qt 5
 * Qt's `serialport`
 * Optional: clang build tools (see below)
 * Optional for clang: [Bear](https://github.com/rizsotto/Bear)

### Example: toolchain setup on Debian

```bash
$ apt install qt5-default libqt5serialport5-dev
$ apt install clang-7 clang-tools-7 clang-tidy-7 clang-format-7
$ apt install bear
```

### Build

Clone this repository (including submodules!):

```
$ git clone --recurse-submodules https://github.com/kmzbrnoI/xn-lib-cpp-qt
```

And then build:

```
$ mkdir build
$ cd build
$ qmake -spec linux-clang ..
$ bear make
```

## Cross-compiling for Windows

This library could be cross-compiled for Windows via [MXE](https://mxe.cc/).
Follow [these instructions](https://stackoverflow.com/questions/14170590/building-qt-5-on-linux-for-windows)
for building standalone `dll` file.

You may want to use similar script as `activate.sh`:

```bash
export PATH="$HOME/...../mxe/usr/bin:$PATH"
~/...../mxe/usr/i686-w64-mingw32.static/qt5/bin/qmake ..
```

Make MXE this way:

```bash
make qtbase qtserialport
```

## Style checking

```
$ clang-tidy-7 -p build -extra-arg-before=-x -extra-arg-before=c++ -extra-arg=-std=c++14 -header-filter=. *.cpp
$ clang-format-7 *.cpp *.h
```

## Authors

This library was created by:

 * Jan Horacek ([jan.horacek@kmz-brno.cz](mailto:jan.horacek@kmz-brno.cz))

Do not hesitate to contact author in case of any troubles!

## License

This application is released under the [Apache License v2.0
](https://www.apache.org/licenses/LICENSE-2.0).
