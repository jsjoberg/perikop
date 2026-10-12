#!/bin/bash
# Runs with the verified macOS dependencies and no network access.
set -euo pipefail
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export ASAN_OPTIONS=halt_on_error=1
cmake -S . -B build/sanitizers -DPERIKOP_DEPENDENCY_DIR="$PWD/build/ci/sources" \
    -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64 -DORTHO_SANITIZERS=ON -DwxBUILD_PRECOMP=OFF
cmake --build build/sanitizers --parallel 3
ctest --test-dir build/sanitizers --output-on-failure --timeout 240
