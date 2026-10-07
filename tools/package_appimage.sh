#!/bin/sh
# Packs a built Linux tree, by default build/cmake, into build/package/Perikop-<version>-x86_64.AppImage.
# linuxdeploy and its GTK plugin copy GTK and the other shared libraries into the image.
set -eu
build=$(realpath "${1:-build/cmake}")
source=$(realpath "$(dirname "$0")/..")
tools=$build/appimage-tools
appdir=$build/AppDir
mkdir -p "$tools" "$source/build/package"
fetch() {
    [ -f "$tools/$1" ] || curl -fsSL -o "$tools/$1" "$2"
    echo "$3  $tools/$1" | sha256sum -c -
    chmod +x "$tools/$1"
}
fetch linuxdeploy-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage \
    c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
fetch linuxdeploy-plugin-gtk.sh \
    https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/3b67a1d1c1b0c8268f57f2bce40fe2d33d409cea/linuxdeploy-plugin-gtk.sh \
    b0f4cbc684a0103a9651f0955b635eaea0096b3a66c0f5a2c2aa337960375171
cp "$source/resources/icons/OrthodoxReader.iconset/icon_256x256.png" "$tools/perikop.png"
rm -rf "$appdir"
DESTDIR="$appdir" cmake --install "$build" --prefix /usr
cd "$source/build/package"
PATH="$tools:$PATH" APPIMAGE_EXTRACT_AND_RUN=1 DEPLOY_GTK_VERSION=3 \
    LINUXDEPLOY_OUTPUT_VERSION=$(sed -n 's/^project(perikop VERSION \([^ ]*\).*/\1/p' "$source/CMakeLists.txt") \
    "$tools/linuxdeploy-x86_64.AppImage" --appdir "$appdir" --executable "$appdir/usr/bin/perikop" \
    --desktop-file "$source/cmake/perikop.desktop" --icon-file "$tools/perikop.png" \
    --plugin gtk --output appimage
