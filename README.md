![Logo](https://user-images.githubusercontent.com/24392180/66733887-b52b4780-ee69-11e9-86ee-ab04caf98287.png)

# zps [![Release](https://img.shields.io/github/release/orhun/zps.svg?style=flat-square)](https://github.com/orhun/zps/releases)

### A small utility for listing or cleaning up zombie processes on GNU/Linux.
[![Travis Build](https://img.shields.io/travis/orhun/zps.svg?style=flat-square)](https://travis-ci.org/orhun/zps)
[![Docker Build](https://img.shields.io/docker/cloud/build/orhunp/zps.svg?style=flat-square)](https://hub.docker.com/r/orhunp/zps/builds)
[![Codacy](https://img.shields.io/codacy/grade/3d40a551806b4c788befba6d2920675b.svg?style=flat-square)](https://www.codacy.com/manual/orhun/zps?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=orhun/zps&amp;utm_campaign=Badge_Grade)
[![Codecov](https://img.shields.io/codecov/c/github/orhun/zps?style=flat-square)](https://codecov.io/gh/orhun/zps)
[![Stars](https://img.shields.io/github/stars/orhun/zps.svg?style=flat-square)](https://github.com/orhun/zps/stargazers) [![License](https://img.shields.io/github/license/orhun/zps.svg?color=blue&style=flat-square)](./LICENSE)

## Installation

### • AUR

### • CMake

```
mkdir -p build && cd build
cmake ../ -DCMAKE_INSTALL_PREFIX=/usr
make
sudo make install
sudo ldconfig
```

### • Make

```
make
sudo make install
```

### • GCC

```
cd src/ && gcc -O3 -Wall zps.c -o zps
```

## Usage

### Command Line Arguments

## TODO(s)

## License

GNU General Public License ([v3](https://www.gnu.org/licenses/gpl.txt))

## Copyright

Copyright (c) 2019, [orhun](https://www.github.com/orhun)
