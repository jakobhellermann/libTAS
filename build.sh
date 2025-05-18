#!/bin/sh

set -e
cd "$(dirname "$0")"

aclocal
autoconf
autoheader
automake --add-missing
mkdir -p build
cd build
CXXFLAGS="-O2 -g -Wall -pedantic" ../configure "$@"
make -j4

# Build .AppImage

wget -nc https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
wget -nc https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage
chmod +x linuxdeploy*.AppImage

APPDIR_PATH=$(readlink -f ./AppDir)

make prefix=/usr "DESTDIR=$APPDIR_PATH" install
NO_STRIP=1 ARCH=x86_64 ./linuxdeploy-x86_64.AppImage --appimage-extract-and-run --appdir AppDir/ --plugin qt --output appimage
