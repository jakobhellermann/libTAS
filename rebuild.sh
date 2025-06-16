#!/bin/sh

set -e
cd "$(dirname "$0")"
cd build

make -j$(nproc)

APPDIR_PATH=$(readlink -f ./AppDir)

make prefix=/usr "DESTDIR=$APPDIR_PATH" install
NO_STRIP=1 ARCH=x86_64 ./linuxdeploy-x86_64.AppImage --appimage-extract-and-run --appdir AppDir/ --plugin qt --output appimage
