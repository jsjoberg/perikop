#!/bin/bash
# Self-contained native update check. Its sibling files are prepared by velopack_macos_test.py.
set -euo pipefail
kit=$(cd "$(dirname "$0")" && pwd)
if [ "${PERIKOP_MACOS_TEST_SANDBOX:-}" != 1 ]; then
    exec /usr/bin/sandbox-exec -p '(version 1)(allow default)(deny network*)' \
        /usr/bin/env PERIKOP_MACOS_TEST_SANDBOX=1 /bin/bash "$0" "$@"
fi
architecture=$(cat "$kit/architecture.txt")
[ "$architecture" = arm64 ] || { echo "Only Apple Silicon is supported" >&2; exit 1; }
system=$(/usr/bin/sw_vers -productVersion)
[ "${system%%.*}" -ge 15 ] || { echo "This package needs macOS 15 or later" >&2; exit 1; }
if [ "${1:-}" = --require-macos15 ]; then
    [ "${system%%.*}" = 15 ] || { echo "The baseline check requires macOS 15; found $system" >&2; exit 1; }
elif [ "$#" != 0 ]; then
    echo "Usage: bash validate_macos_update.command [--require-macos15]" >&2
    exit 1
fi
cd "$kit"
exec > >(tee "$kit/result-$architecture.txt") 2>&1
/usr/bin/shasum -a 256 -c SHA256SUMS.txt
cat SHA256SUMS.txt
work=$(mktemp -d "${TMPDIR:-/tmp}/perikop-macos-update.XXXXXX")
trap 'echo "Diagnostic files kept in $work"' EXIT
echo "macOS $system; host $(/usr/bin/uname -m); target $architecture; networking denied"
# The SDK stores packages under HOME/Library/Caches/velopack. Keep all test state private.
export HOME="$work/home" TMPDIR="$work/tmp/"
mkdir -p "$HOME" "$TMPDIR" "$work/profile" "$work/installed"
/usr/sbin/pkgutil --expand-full "$kit/first.pkg" "$work/expanded"
/usr/bin/ditto "$work/expanded/1.pkg/Payload/Perikop.app" "$work/installed/Perikop.app"
app="$work/installed/Perikop.app"
program="$app/Contents/MacOS/Perikop"
database="$work/profile/user.db"
/usr/bin/sqlite3 "$database" <<'SQL'
CREATE TABLE settings(key TEXT PRIMARY KEY NOT NULL,value TEXT NOT NULL) STRICT;
CREATE TABLE progress(key TEXT PRIMARY KEY NOT NULL,completed TEXT NOT NULL) STRICT;
CREATE TABLE reading_position(key TEXT PRIMARY KEY NOT NULL,section INTEGER NOT NULL,book TEXT NOT NULL,source TEXT NOT NULL,chapter INTEGER NOT NULL,verse INTEGER NOT NULL,suffix TEXT NOT NULL,percent INTEGER NOT NULL) STRICT;
INSERT INTO settings VALUES('update_channel','nightly'),('theme','dark'),('color_scheme','nord');
INSERT INTO progress VALUES('update-test','2026-10-18');
INSERT INTO reading_position VALUES('update-test',0,'John','sv1917',1,3,'',25);
CREATE TABLE bookmark(id INTEGER PRIMARY KEY,source TEXT NOT NULL,book TEXT NOT NULL,
first_chapter INTEGER NOT NULL,first_verse INTEGER NOT NULL,first_suffix TEXT NOT NULL,
last_chapter INTEGER NOT NULL,last_verse INTEGER NOT NULL,last_suffix TEXT NOT NULL,
UNIQUE(source,book,first_chapter,first_verse,first_suffix,last_chapter,last_verse,last_suffix)) STRICT;
INSERT INTO bookmark VALUES(1,'grc-lxx','Ps',22,1,'',22,6,'');
SQL
# Compare saved values, allowing the application to add tables or schema objects.
profile_snapshot() {
    /usr/bin/sqlite3 -bail -quote "$database" <<'SQL'
SELECT key,value FROM settings ORDER BY key;
SELECT key,completed FROM progress ORDER BY key;
SELECT key,section,book,source,chapter,verse,suffix,percent FROM reading_position ORDER BY key;
SELECT id,source,book,first_chapter,first_verse,first_suffix,last_chapter,last_verse,last_suffix FROM bookmark ORDER BY id;
SQL
}
before=$(profile_snapshot)
verify_bundle() {
    /usr/bin/codesign --verify --deep --strict "$app"
    for binary in "$program" "$app/Contents/MacOS/UpdateMac" "$app"/Contents/Frameworks/*.dylib; do
        description=$(/usr/bin/file -L -b "$binary")
        [[ "$description" == "Mach-O 64-bit "* && "$description" == *" $architecture" ]] || {
            echo "Wrong architecture: $binary" >&2; return 1;
        }
    done
    [ "$(/usr/libexec/PlistBuddy -c 'Print LSMinimumSystemVersion' "$app/Contents/Info.plist")" = 15.0 ]
}
feed="$kit/feed"
probe() {
    /usr/bin/arch "-$architecture" "$program" --profile "$work/profile" --update-probe "$feed"
}
wait_for_version() {
    deadline=$((SECONDS + 120))
    while true; do
        # Wait without starting another copy that could apply the same update again.
        if [ "$(/usr/libexec/PlistBuddy -c 'Print CFBundleVersion' "$app/Contents/Info.plist" 2>/dev/null || true)" = "$1" ] \
                && /usr/bin/codesign --verify --deep --strict "$app" > "$work/replacement.log" 2>&1; then
            break
        fi
        if [ "$SECONDS" -ge "$deadline" ]; then
            cat "$work/replacement.log" 2>/dev/null || true
            echo "Timed out waiting for UpdateMac to replace the bundle" >&2
            exit 1
        fi
        sleep 2
    done
}
verify_bundle
probe > "$work/first.log" 2>&1
cat "$work/first.log"
for line in 'VERSION 1.0.0-test.1' 'AVAILABLE 1.0.0-test.2' APPLYING; do
    /usr/bin/grep -qx "$line" "$work/first.log"
done
wait_for_version 1.0.0-test.2
probe > "$work/updated.log" 2>&1
cat "$work/updated.log"
/usr/bin/grep -qx 'VERSION 1.0.0-test.2' "$work/updated.log"
/usr/bin/grep -qx NO_UPDATE "$work/updated.log"
verify_bundle
[ "$(/usr/libexec/PlistBuddy -c 'Print CFBundleVersion' "$app/Contents/Info.plist")" = 1.0.0-test.2 ]
feed="$kit/delta-feed"
probe > "$work/delta.log" 2>&1
cat "$work/delta.log"
for line in 'VERSION 1.0.0-test.2' 'AVAILABLE 1.0.0-test.3' APPLYING; do
    /usr/bin/grep -qx "$line" "$work/delta.log"
done
wait_for_version 1.0.0-test.3
probe > "$work/third.log" 2>&1
cat "$work/third.log"
/usr/bin/grep -qx 'VERSION 1.0.0-test.3' "$work/third.log"
/usr/bin/grep -qx NO_UPDATE "$work/third.log"
verify_bundle
[ "$(profile_snapshot)" = "$before" ] || { echo "Saved profile data changed during update" >&2; exit 1; }
/usr/bin/arch "-$architecture" "$program" --smoke-test
rm -rf "$app"
[ "$(profile_snapshot)" = "$before" ] || { echo "Saved profile data changed during removal" >&2; exit 1; }
echo "PASS: full update to 1.0.0-test.2, delta update to 1.0.0-test.3 without its full package; signatures, UI and profile kept"
trap - EXIT
rm -rf "$work"
