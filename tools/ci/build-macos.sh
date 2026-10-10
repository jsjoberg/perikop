#!/bin/bash
# Runs with sandbox-exec denying network access.
set -euo pipefail
export PATH="$PWD/build/ci/tools/bin:$PATH"
export CCACHE_DIR="$PWD/build/ccache" CCACHE_BASEDIR="$PWD" CCACHE_COMPILERCHECK=content CCACHE_MAXSIZE=2G
llvm="$PWD/build/ci/tools/bin"
cmake -S . -B build/cmake "-DPERIKOP_DEPENDENCY_DIR=$PWD/build/ci/sources" \
    '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' '-DCMAKE_TEST_LAUNCHER=/usr/bin/arch;-arm64' \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DwxBUILD_PRECOMP=OFF \
    "-DORTHO_CLANG_TIDY=$llvm/clang-tidy" "-DORTHO_RUN_CLANG_TIDY=$llvm/run-clang-tidy"
cmake --build build/cmake --parallel 3
python3 tools/check_macos_arch.py build/cmake/bin/Perikop.app
ctest --test-dir build/cmake --output-on-failure
# Execute the Intel slices, including the UI and speech tests, through Rosetta.
cmake -S . -B build/cmake '-DCMAKE_TEST_LAUNCHER=/usr/bin/arch;-x86_64'
ctest --test-dir build/cmake --output-on-failure
cmake --build build/cmake --target tidy
python3 tools/check_resources.py
python3 tests/icon_test.py
python3 tests/linux_abi_test.py
python3 tests/ci_dependencies_test.py
python3 tests/macos_arch_test.py
if [ "${PACKAGE:-false}" = true ]; then
    for attempt in 1 2 3; do
        cpack --config build/cmake/CPackConfig.cmake -B build/package && break
        [ "$attempt" = 3 ] && exit 1
        sleep 15
    done
    mount="$PWD/build/dmg-check"
    mkdir -p "$mount"
    hdiutil attach -readonly -nobrowse -mountpoint "$mount" build/package/Perikop-*-macOS-universal.dmg
    trap 'hdiutil detach "$mount"' EXIT
    python3 tools/check_macos_arch.py "$mount/Perikop.app"
    codesign --verify --deep --strict "$mount/Perikop.app"
    for architecture in arm64 x86_64; do
        /usr/bin/arch "-$architecture" "$mount/Perikop.app/Contents/MacOS/Perikop" \
            --smoke-test --resources "$mount/Perikop.app/Contents/Resources"
    done
    hdiutil detach "$mount"
    trap - EXIT
fi
ccache --show-stats
