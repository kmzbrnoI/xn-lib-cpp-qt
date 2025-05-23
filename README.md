# C++ Qt XpressNET library

This repository contains a fully open-source C++ library for communicating
with [XpressNET](https://dccwiki.com/XpressNet_Protocol)
[DCC Command Station](https://dccwiki.com/Command_station).

It connects to the (virtual) serial port, which is typically served by LI.

Currently, it implements basic commands of [XpressNET
v3.0](http://www.lenzusa.com/1newsite1/Manuals/xpressnet.pdf) protocol as well
as some advanced commands of [XpressNET
v3.6](https://www.lenz-elektronik.de/pdf/XpressNet%20und%20USB%20Interface.pdf)).

See `xn.h: class XpressNET` for list of supported commands.

## Usage

You may use this library in two major ways:

### Dynamic library

You can download precompiled `dll` version of this library in *Releases* section.

Since v2.0 release, this library is distributed as `trakce-xn.dll` file.
This library is developed in Qt framework, thus some more dll are required on
your computer for `trakce-xn.dll` to work. These libraries are available in
*Relaeses* in zips `trakce-xn-required-libs-*.zip`. Use `min` variant by default,
this should be enough. If your computer complains about unmet dependencies, use
`all` variant.

Dynamic-library-api specification is located on
[wiki](https://github.com/kmzbrnoI/xn-lib-cpp-qt/wiki).

### Static library

Simply include header files listed in `xn.pro` into your project and use
instance of `XpressNet` class (see `xn.h`).

Add `.cpp` and `.h` files from `xn.pro` to `yourproject.pro`. `xn.pro` is not
used in your project. You just include header files as in plain C.

This library uses [Qt](https://www.qt.io/)'s
[SerialPort](http://doc.qt.io/qt-5/qtserialport-index.html) which creates a
very good cross-platform abstraction of serial port interface. Thus, the
library uses Qt's mechanisms like slots and signals.

When library is used as static library, it is **not** usable without Qt.

## Basic information

 * This library uses 28 speed steps only. It simplifies things a lot. Other
   speed steps could be added in future.
 * To change the version of this library, update both constants at `xn.pro`
   file and `xn.h` file. This is needed for proper behavior as a dynamic and
   static library too.

## Project structure

 * `xn*{.cpp,.h}` files contain low-level implementations of XpressNET library.
   Only these files are needed for static-library use.
   - See `xn.h` for static API specification.
 * `lib-*{.cpp,.h}` files contain implementations of dynamic-library API. These
   files use `xn*{.cpp,.h}` files.
   - See `lib-main.h` for dynamic API specification.

## XpressNET API

This library has been tested with:

 * [uLI](https://uli.kmz-brno.cz/)
 * Lenz LI-USB-Ethernet
 * NanoX's LI

It should support all LI100-like or LI101-like interfaces. It also supports
newer interfaces which add `0xFF 0xFE` to the header of each packet. User
chooses which version of protocol the library should use.

## Building & toolkit

This SW was developed in `vim` using `qmake` & `make`. Downloads are available
in *Releases* section. It is suggested to use `clang` as a compiler, because
then you may use `clang-tools` during development process.

### Prerequisities

 * Qt 6
 * Qt's `serialport`
 * Optional: clang build tools
 * Optional for clang: [Bear](https://github.com/rizsotto/Bear)

### Build

Clone this repository:

```
$ git clone https://github.com/kmzbrnoI/xn-lib-cpp-qt
```

And then build:

```
$ mkdir build
$ cd build
$ qmake -spec linux-clang ..
$ bear make
```

## Compiling for Windows

Just open the project in Qt Creator and compile it. This approach is currently used to build windows binaries in releases.

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
