#!/bin/bash
# Self-contained native update check. Its sibling files are prepared by velopack_macos_test.py.
set -euo pipefail
kit=$(cd "$(dirname "$0")" && pwd)
if [ "${PERIKOP_MACOS_TEST_SANDBOX:-}" != 1 ]; then
    exec /usr/bin/sandbox-exec -p '(version 1)(allow default)(deny network*)' \
        /usr/bin/env PERIKOP_MACOS_TEST_SANDBOX=1 /bin/bash "$0" "$@"
fi
architecture=$(cat "$kit/architecture.txt")
case "$architecture" in arm64|x86_64) ;; *) echo "Invalid test architecture" >&2; exit 1 ;; esac
system=$(/usr/bin/sw_vers -productVersion)
if [ "${1:-}" = --require-macos13 ]; then
    case "$system" in
        13.[0123]|13.[0123].*) echo "This package needs macOS 13.4 or later" >&2; exit 1 ;;
        13.*) ;;
        *) echo "The macOS 13 check requires macOS 13.4 or later in the 13 series; found $system" >&2; exit 1 ;;
    esac
elif [ "$#" != 0 ]; then
    echo "Usage: bash validate_macos_update.command [--require-macos13]" >&2
    exit 1
fi
cd "$kit"
/usr/bin/shasum -a 256 -c SHA256SUMS.txt
work=$(mktemp -d "${TMPDIR:-/tmp}/perikop-macos-update.XXXXXX")
trap 'echo "Diagnostic files kept in $work"' EXIT
exec > >(tee "$kit/result-$architecture.txt") 2>&1
echo "macOS $system; target $architecture; networking denied"
# The SDK stores packages under HOME/Library/Caches/velopack. Keep all test state private.
export HOME="$work/home"
mkdir -p "$HOME" "$work/profile" "$work/installed"
/usr/sbin/pkgutil --expand-full "$kit/first.pkg" "$work/expanded"
/usr/bin/ditto "$work/expanded/1.pkg/Payload/Perikop.app" "$work/installed/Perikop.app"
app="$work/installed/Perikop.app"
program="$app/Contents/MacOS/Perikop"
database="$work/profile/user.db"
/usr/bin/sqlite3 "$database" <<'SQL'
CREATE TABLE settings(key TEXT PRIMARY KEY NOT NULL,value TEXT NOT NULL) STRICT;
CREATE TABLE progress(key TEXT PRIMARY KEY NOT NULL,completed TEXT NOT NULL) STRICT;
INSERT INTO settings VALUES('update_channel','nightly'),('theme','dark'),('color_scheme','nord');
INSERT INTO progress VALUES('update-test','2026-10-18');
SQL
before=$(/usr/bin/sqlite3 "$database" .dump)
verify_bundle() {
    /usr/bin/codesign --verify --deep --strict "$app"
    for binary in "$program" "$app/Contents/MacOS/UpdateMac" "$app"/Contents/Frameworks/*.dylib; do
        [ "$('/usr/bin/lipo' -archs "$binary")" = "$architecture" ] || {
            echo "Wrong architecture: $binary" >&2; return 1;
        }
    done
    [ "$(/usr/libexec/PlistBuddy -c 'Print LSMinimumSystemVersion' "$app/Contents/Info.plist")" = 13.4 ]
}
probe() {
    /usr/bin/arch "-$architecture" "$program" --profile "$work/profile" --update-probe "$kit/feed"
}
verify_bundle
probe > "$work/first.log" 2>&1
cat "$work/first.log"
for line in 'VERSION 1.0.0-test.1' 'AVAILABLE 1.0.0-test.2' APPLYING; do
    /usr/bin/grep -qx "$line" "$work/first.log"
done
deadline=$((SECONDS + 120))
while true; do
    if probe > "$work/updated.log" 2>&1 && /usr/bin/grep -qx 'VERSION 1.0.0-test.2' "$work/updated.log"; then
        break
    fi
    if [ "$SECONDS" -ge "$deadline" ]; then
        cat "$work/updated.log"
        echo "Timed out waiting for UpdateMac to replace the bundle" >&2
        exit 1
    fi
    sleep 2
done
cat "$work/updated.log"
/usr/bin/grep -qx NO_UPDATE "$work/updated.log"
verify_bundle
[ "$(/usr/libexec/PlistBuddy -c 'Print CFBundleVersion' "$app/Contents/Info.plist")" = 1.0.0-test.2 ]
[ "$(/usr/bin/sqlite3 "$database" .dump)" = "$before" ]
/usr/bin/arch "-$architecture" "$program" --smoke-test
rm -rf "$app"
[ "$(/usr/bin/sqlite3 "$database" .dump)" = "$before" ]
echo "PASS: installed 1.0.0-test.1, updated to 1.0.0-test.2, verified signatures and UI, removed the app; profile kept"
trap - EXIT
rm -rf "$work"
