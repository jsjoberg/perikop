#!/bin/sh
# Packs a built Linux tree, by default build/cmake, into build/package/Perikop-<version>-<arch>.AppImage,
# for x86_64 or aarch64. The aarch64 image is the Raspberry Pi 5 package (64-bit Raspberry Pi OS).
# linuxdeploy and its GTK plugin copy GTK and the other shared libraries into the image.
set -eu
build=$(realpath "${1:-build/cmake}")
source=$(realpath "$(dirname "$0")/..")
tools=${PERIKOP_APPIMAGE_TOOLS:-$build/appimage-tools}
appdir=$build/AppDir
mkdir -p "$tools" "$source/build/package"
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
x86_64)
    linuxdeploy_hash=c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
    runtime_hash=156f4bdbde9c52d01814600013e0a273f0118dc2de98975f3c8c63427ec79074 ;;
aarch64)
    linuxdeploy_hash=620095110d693282b8ebeb244a95b5e911cf8f65f76c88b4b47d16ae6346fcff
    runtime_hash=b4ff0030242d0c3bb12ce40541828303cf167493f4793456f0436edd6255c39d ;;
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
fetch "runtime-$arch" \
    "https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-$arch" \
    "$runtime_hash"
cp "$source/resources/icons/OrthodoxReader.iconset/icon_256x256.png" "$tools/perikop.png"
rm -rf "$appdir"
DESTDIR="$appdir" cmake --install "$build" --prefix /usr --strip
cd "$source/build/package"
linuxdeploy() {
    PATH="$tools:$PATH" APPIMAGE_EXTRACT_AND_RUN=1 DEPLOY_GTK_VERSION=3 LDAI_RUNTIME_FILE="$tools/runtime-$arch" \
        LINUXDEPLOY_OUTPUT_VERSION=$(sed -n 's/^project(perikop VERSION \([^ ]*\).*/\1/p' "$source/CMakeLists.txt") \
        "$tools/linuxdeploy-$arch.AppImage" --appdir "$appdir" "$@"
}
linuxdeploy --executable "$appdir/usr/bin/perikop" \
    --library "$appdir/usr/bin/libstdc++.so.6" --library "$appdir/usr/bin/libgcc_s.so.1" \
    --desktop-file "$source/cmake/perikop.desktop" --icon-file "$tools/perikop.png" --plugin gtk
# The GTK plugin forces X11. Under Wayland that means XWayland, which made the reader unusable on a
# Raspberry Pi 5, so let GTK choose: Wayland when available, else X11, or the user's own GDK_BACKEND.
hook="$appdir/apprun-hooks/linuxdeploy-plugin-gtk.sh"
grep -q '^export GDK_BACKEND=x11' "$hook" || { echo "GTK hook no longer forces X11; review $hook" >&2; exit 1; }
sed -i '/^export GDK_BACKEND=x11/d' "$hook"
# Ship the licence files of every system package whose libraries linuxdeploy bundled, beside the app's own.
licenses=$appdir/usr/bin/resources/licenses/linux
mkdir -p "$licenses"
rm -f "$build/appimage-unlicensed.txt"
# libstdc++ and libgcc_s come from the GCC toolset, whose licences the app already ships.
find "$appdir/usr/lib" -name '*.so.*' ! -name 'libstdc++.so.*' ! -name 'libgcc_s.so.*' -printf '%f\n' |
    sort -u | while read -r library; do
    host=$(find /usr/lib64 /lib64 -maxdepth 3 -name "$library" -print -quit 2>/dev/null)
    [ -n "$host" ] && rpm -qf --qf '%{NAME}\n' "$host" 2>/dev/null || true
done | sort -u | while read -r package; do
    # Group by source package: subpackages such as cairo-gobject keep their licence text in cairo.
    rpm_source=$(rpm -q --qf '%{SOURCERPM}' "$package")
    origin=$(echo "$rpm_source" | sed 's/-[^-]*-[^-]*\.src\.rpm$//')
    siblings=$(rpm -qa --qf '%{NAME} %{SOURCERPM}\n' | awk -v s="$rpm_source" '$2 == s { print $1 }')
    rpm -q --qf '%{NAME} %{VERSION}-%{RELEASE}: %{LICENSE}\n' "$package" >> "$licenses/PACKAGES.txt"
    { rpm -q --licensefiles $siblings || true
      rpm -q --docfiles $siblings | grep -i -E '/(COPYING|LICEN[CS]E|COPYRIGHT|NOTICE|BSD)[^/]*$' || true
    } | grep '^/' | while read -r file; do
        [ -f "$file" ] && { mkdir -p "$licenses/$origin"; cp "$file" "$licenses/$origin/"; }
    done
    # Rocky ships no licence text for some packages; tools/appimage-licenses has upstream's.
    [ -d "$source/tools/appimage-licenses/$origin" ] && cp -r "$source/tools/appimage-licenses/$origin" "$licenses/"
    [ -d "$licenses/$origin" ] || [ "$(rpm -q --qf '%{LICENSE}' "$package")" = "Public Domain" ] ||
        echo "$package" >> "$build/appimage-unlicensed.txt"
done
if [ -s "$build/appimage-unlicensed.txt" ]; then
    echo "No licence text for bundled packages:" $(cat "$build/appimage-unlicensed.txt") >&2
    exit 1
fi
linuxdeploy --output appimage
# Also check GTK libraries and AppRun added by the packaging tools.
python3 "$source/tools/check_linux_abi.py" "$appdir"
