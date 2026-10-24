#!/bin/sh
# Packs the AppDir that tools/prepare_appdir.sh made into Velopack's AppImage and update package:
#   tools/package_velopack.sh <build dir> <version> <channel> <output dir>
# Linux channels include the glibc baseline, such as linux-x64-glibc241-nightly; see docs/updating.md.
set -eu
build=$(realpath "$1")
version=$2
channel=$3
output=$4
source=$(realpath "$(dirname "$0")/..")
vpk=${PERIKOP_VPK:-$source/build/ci/tools/vpk}
case $(uname -m) in
x86_64) runtime=linux-x64 updater=UpdateNix_x64 ;;
aarch64) runtime=linux-arm64 updater=UpdateNix_arm64 ;;
*)
    echo "No Velopack package for $(uname -m)" >&2
    exit 1 ;;
esac
# Velopack's updater ships inside the AppImage, so it must meet the same glibc baseline as Perikop.
python3 "$source/tools/check_linux_abi.py" "$vpk/vendor/$updater"
# vpk keeps a finished AppDir as it is when the directory name ends in .AppDir.
ln -sfn AppDir "$build/Perikop.AppDir"
mkdir -p "$output"
dotnet "$vpk/tools/net8.0/any/vpk.dll" pack --packId Perikop --packTitle Perikop --packVersion "$version" \
    --packDir "$build/Perikop.AppDir" --mainExe perikop --runtime "$runtime" --channel "$channel" \
    --delta "${PERIKOP_VELOPACK_DELTA:-None}" --outputDir "$output"
