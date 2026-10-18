#!/bin/bash
# Runs inside the pinned image with Docker networking disabled.
set -euo pipefail
# Docker runs as root, but the mounted checkout belongs to the runner. CMake needs its Git
# metadata for branch/tag versions and the commit shown in About. Trust only this checkout.
git config --global --add safe.directory "$PWD"
export CCACHE_DIR=/work/build/ccache CCACHE_BASEDIR=/work CCACHE_COMPILERCHECK=content CCACHE_MAXSIZE=2G
export PERIKOP_OFFLINE=1 PERIKOP_APPIMAGE_TOOLS=/work/build/ci/tools/appimage
compiler=/opt/rh/gcc-toolset-15/root/usr/bin
test "$("$compiler/g++" -dumpversion | cut -d . -f 1)" = 15
cmake -S . -B build/cmake -DPERIKOP_DEPENDENCY_DIR=/work/build/ci/sources \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DwxBUILD_PRECOMP=OFF
cmake --build build/cmake --parallel 3
python3 tools/check_linux_abi.py build/cmake/bin
if ! xvfb-run -a -s '-screen 0 1600x1200x24' ctest --test-dir build/cmake --output-on-failure; then
    mkdir -p build/smoke
    xvfb-run -a -s '-screen 0 1600x1200x24' gdb -batch -ex run -ex bt --args build/cmake/bin/perikop --smoke-test \
        --resources build/cmake/bin/resources --screenshot /work/build/smoke/reader.png || true
    exit 1
fi
# EL9's Python links an older system SQLite that cannot read STRICT tables.
# Use the same verified SQLite source as the reader for this resource check.
gcc -shared -fPIC build/ci/sources/sqlite/sqlite3.c -o build/ci/libsqlite3.so -ldl -lpthread
LD_PRELOAD="$PWD/build/ci/libsqlite3.so" python3 -c 'import sqlite3; assert sqlite3.sqlite_version_info >= (3, 37, 0)'
LD_PRELOAD="$PWD/build/ci/libsqlite3.so" python3 tools/check_resources.py
python3 tests/modern_lexicon_test.py
python3 tests/icon_test.py
python3 tests/linux_abi_test.py
python3 tests/ci_dependencies_test.py
python3 tests/windows_cross_test.py
python3 tests/appimage_licenses_test.py
python3 tests/nightly_test.py
python3 tests/update_packages_test.py
if [ "${PACKAGE:-false}" = true ]; then
    tools/package_appimage.sh build/cmake
    xvfb-run -a -s '-screen 0 1600x1200x24' build/cmake/AppDir/usr/bin/perikop \
        --resources build/cmake/AppDir/usr/share/perikop/resources --speech-probe /work/build/voice-linux.wav
    # Install the AppDir with Velopack and let the program update itself from a local feed.
    LD_PRELOAD="$PWD/build/ci/libsqlite3.so" python3 tools/ci/velopack_linux_test.py build/cmake
    version=$(sed -n 's/^#define PERIKOP_VERSION "\([^"]*\)"/\1/p' build/cmake/generated/perikop_version.hpp)
    case $(uname -m) in x86_64) runtime=x64 ;; aarch64) runtime=arm64 ;; esac
    track=releases
    [ "${NIGHTLY:-false}" != true ] || track=nightly
    channel="linux-$runtime-$track"
    output="build/velopack/$channel"
    python3 tools/ci/update_packages.py seed "build/update-base/$channel" "$output"
    PERIKOP_VELOPACK_DELTA=BestSpeed tools/package_velopack.sh build/cmake "$version" "$channel" "$output"
    python3 tools/ci/update_packages.py finish "$output" "$channel" "$version" build/publish/updates
    mkdir -p build/publish/legacy
    cp build/package/Perikop-* build/publish/legacy/
    # Both AppImages start with the pinned runtimes and carry the runtime licences (libfuse is LGPL-2.1).
    python3 tools/appimage_runtime.py check build/cmake
    # The runtimes' corresponding source ships beside the AppImages in every release.
    python3 tools/appimage_runtime.py source --output build/publish/legacy
fi
ccache --show-stats
