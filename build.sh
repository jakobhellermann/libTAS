#!/bin/sh

set -e
cd "$(dirname "$0")"

cmake -B build
cmake --build build -j"$(nproc)"

# Build .AppImage

# wget -nc https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
# chmod +x linuxdeploy*.AppImage

# APPDIR_PATH=$(readlink -f ./AppDir)

# make prefix=/usr DESTDIR=$APPDIR_PATH install
# ARCH=x86_64 ./linuxdeploy*.AppImage --appimage-extract-and-run --appdir AppDir/ --output appimage
