#!/bin/sh

set -e
cd "$(dirname "$0")"

cmake -B build
cmake --build build -j"$(nproc)"

# Build .AppImage

wget -nc https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
wget -nc https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage

chmod +x linuxdeploy*.AppImage

APPDIR_PATH=$(readlink -f ./AppDir)
DESTDIR="$APPDIR_PATH" cmake --install build --config Release --prefix /usr

NO_STRIP=true ARCH=x86_64 ./linuxdeploy-x86_64.AppImage --appimage-extract-and-run --appdir AppDir/ --plugin qt --output appimage
