#!/bin/bash
# Runs inside the pinned image with Docker networking disabled.
set -euo pipefail
export CCACHE_DIR=/work/build/ccache CCACHE_BASEDIR=/work CCACHE_COMPILERCHECK=content CCACHE_MAXSIZE=2G
export PERIKOP_OFFLINE=1 PERIKOP_APPIMAGE_TOOLS=/work/build/ci/tools/appimage
compiler=/opt/rh/gcc-toolset-15/root/usr/bin
test "$("$compiler/g++" -dumpversion | cut -d . -f 1)" = 15
cmake -S . -B build/cmake -DPERIKOP_DEPENDENCY_DIR=/work/build/ci/sources \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DwxBUILD_PRECOMP=OFF
cmake --build build/cmake --parallel 3
python3 tools/check_linux_abi.py build/cmake/bin
if ! xvfb-run -a ctest --test-dir build/cmake --output-on-failure; then
    mkdir -p build/smoke
    xvfb-run -a gdb -batch -ex run -ex bt --args build/cmake/bin/perikop --smoke-test \
        --resources build/cmake/bin/resources --screenshot /work/build/smoke/reader.png || true
    exit 1
fi
# EL9's Python links an older system SQLite that cannot read STRICT tables.
# Use the same verified SQLite source as the reader for this resource check.
gcc -shared -fPIC build/ci/sources/sqlite/sqlite3.c -o build/ci/libsqlite3.so -ldl -lpthread
LD_PRELOAD="$PWD/build/ci/libsqlite3.so" python3 -c 'import sqlite3; assert sqlite3.sqlite_version_info >= (3, 37, 0)'
LD_PRELOAD="$PWD/build/ci/libsqlite3.so" python3 tools/check_resources.py
python3 tests/icon_test.py
python3 tests/linux_abi_test.py
python3 tests/ci_dependencies_test.py
if [ "${PACKAGE:-false}" = true ]; then
    tools/package_appimage.sh build/cmake
fi
ccache --show-stats
