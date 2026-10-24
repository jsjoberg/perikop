#!/bin/sh
# Prepares a built Linux tree, by default build/cmake, as build/cmake/AppDir for x86_64 or aarch64;
# tools/package_velopack.sh then packs it into the AppImage. The aarch64 image is the Raspberry Pi 5
# package (64-bit Raspberry Pi OS). linuxdeploy and its GTK plugin copy GTK and the other shared
# libraries into the AppDir.
set -eu
build=$(realpath "${1:-build/cmake}")
source=$(realpath "$(dirname "$0")/..")
tools=${PERIKOP_APPIMAGE_TOOLS:-$build/appimage-tools}
appdir=$build/AppDir
mkdir -p "$tools"
fetch() {
    if [ ! -f "$tools/$1" ]; then
        if [ "${PERIKOP_OFFLINE:-0}" = 1 ]; then
            echo "Missing offline AppImage tool: $1" >&2
            exit 1
        fi
        curl -fsSL -o "$tools/$1" "$2"
    fi
    echo "$3  $tools/$1" | sha256sum -c -
    chmod +x "$tools/$1"
}
# The same pins as ci/upstreams.json.
arch=$(uname -m)
case $arch in
x86_64) linuxdeploy_hash=c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d ;;
aarch64) linuxdeploy_hash=620095110d693282b8ebeb244a95b5e911cf8f65f76c88b4b47d16ae6346fcff ;;
*)
    echo "No AppImage tools for $arch" >&2
    exit 1 ;;
esac
fetch "linuxdeploy-$arch.AppImage" \
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-$arch.AppImage" \
    "$linuxdeploy_hash"
fetch linuxdeploy-plugin-gtk.sh \
    https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/3b67a1d1c1b0c8268f57f2bce40fe2d33d409cea/linuxdeploy-plugin-gtk.sh \
    b0f4cbc684a0103a9651f0955b635eaea0096b3a66c0f5a2c2aa337960375171
cp "$source/resources/icons/OrthodoxReader.iconset/icon_256x256.png" "$tools/perikop.png"
rm -rf "$appdir"
DESTDIR="$appdir" cmake --install "$build" --prefix /usr --strip
linuxdeploy() {
    PATH="$tools:$PATH" APPIMAGE_EXTRACT_AND_RUN=1 DEPLOY_GTK_VERSION=3 \
        "$tools/linuxdeploy-$arch.AppImage" --appdir "$appdir" "$@"
}
linuxdeploy --executable "$appdir/usr/bin/perikop" \
    --library "$appdir/usr/bin/libstdc++.so.6" --library "$appdir/usr/bin/libgcc_s.so.1" \
    --desktop-file "$source/cmake/perikop.desktop" --icon-file "$tools/perikop.png" --plugin gtk
# Icon themes such as Adwaita 46 ship only SVG icons, and GTK aborts when it cannot load one.
find "$appdir/usr/lib" -name loaders.cache -exec grep -l svg {} + | grep -q . ||
    { echo "The AppDir has no SVG image loader; install librsvg2-common" >&2; exit 1; }
# The GTK plugin forces X11. Under Wayland that means XWayland, which made the reader unusable on a
# Raspberry Pi 5, so let GTK choose: Wayland when available, else X11, or the user's own GDK_BACKEND.
hook="$appdir/apprun-hooks/linuxdeploy-plugin-gtk.sh"
grep -q '^export GDK_BACKEND=x11' "$hook" || { echo "GTK hook no longer forces X11; review $hook" >&2; exit 1; }
sed -i '/^export GDK_BACKEND=x11/d' "$hook"
# Ship the copyright file of every Debian package whose libraries linuxdeploy bundled, beside the app's own.
docs=/usr/share/doc
licenses=$appdir/usr/bin/resources/licenses/linux
mkdir -p "$licenses"
rm -f "$build/appimage-unlicensed.txt"
# libstdc++ and libgcc_s come from GCC, whose licences the app already ships.
find "$appdir/usr/lib" -name '*.so.*' ! -name 'libstdc++.so.*' ! -name 'libgcc_s.so.*' -printf '%f\n' |
    sort -u | while read -r library; do
    # A failed lookup and a path with only a diversion both leave no owner.
    owners=$(dpkg -S "*/$library" 2>/dev/null | grep -v '^diversion ' | cut -d: -f1)
    if [ -n "$owners" ]; then
        printf '%s\n' "$owners"
    else
        echo "$library (no Debian package owner)" >> "$build/appimage-unlicensed.txt"
    fi
done | sort -u | while read -r package; do
    # Group by source package: binary packages such as libcairo-gobject2 share cairo's copyright.
    origin=$(dpkg-query -W -f '${source:Package}' "$package")
    dpkg-query -W -f '${Package} ${Version}\n' "$package" >> "$licenses/PACKAGES.txt"
    if [ -f "$docs/$package/copyright" ]; then
        mkdir -p "$licenses/$origin"
        cp "$docs/$package/copyright" "$licenses/$origin/copyright"
    else
        echo "$package" >> "$build/appimage-unlicensed.txt"
    fi
done
if [ -s "$build/appimage-unlicensed.txt" ]; then
    echo "No licence text for bundled packages:" $(cat "$build/appimage-unlicensed.txt") >&2
    exit 1
fi
# Also check GTK libraries and AppRun added by the packaging tools.
python3 "$source/tools/check_linux_abi.py" "$appdir"
