#!/bin/bash
# Runs with sandbox-exec denying network access.
set -euo pipefail
export PATH="$PWD/build/ci/tools/bin:$PATH"
export CCACHE_DIR="$PWD/build/ccache" CCACHE_BASEDIR="$PWD" CCACHE_COMPILERCHECK=content CCACHE_MAXSIZE=2G
llvm="$PWD/build/ci/tools/bin"
cmake -S . -B build/cmake "-DPERIKOP_DEPENDENCY_DIR=$PWD/build/ci/sources" \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DwxBUILD_PRECOMP=OFF \
    "-DORTHO_CLANG_TIDY=$llvm/clang-tidy" "-DORTHO_RUN_CLANG_TIDY=$llvm/run-clang-tidy"
cmake --build build/cmake --parallel 3
ctest --test-dir build/cmake --output-on-failure
cmake --build build/cmake --target tidy
python3 tools/check_resources.py
python3 tests/icon_test.py
python3 tests/linux_abi_test.py
python3 tests/ci_dependencies_test.py
if [ "${PACKAGE:-false}" = true ]; then
    for attempt in 1 2 3; do
        cpack --config build/cmake/CPackConfig.cmake -B build/package && break
        [ "$attempt" = 3 ] && exit 1
        sleep 15
    done
fi
ccache --show-stats
