#!/bin/bash
# Compile offline on Linux. Windows runs the tests and creates the installer.
set -euo pipefail
export CCACHE_DIR=/work/build/ccache CCACHE_BASEDIR=/work CCACHE_COMPILERCHECK=content CCACHE_MAXSIZE=2G
trap 'ccache --show-stats || true' EXIT
cmake -S . -B build/cmake -G Ninja --toolchain cmake/llvm-mingw.cmake \
    -DPERIKOP_DEPENDENCY_DIR=/work/build/ci/sources \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DwxBUILD_PRECOMP=OFF
cmake --build build/cmake --parallel 4
# Do not transfer CMake's host-specific cache, test paths, or install scripts.
# Windows configures those locally and uses these binaries without rebuilding them.
COPYFILE_DISABLE=1 tar -czf build/windows-x64.tar.gz -C build/cmake bin generated/perikop_version.hpp
