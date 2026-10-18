#!/bin/bash
# Runs with sandbox-exec denying network access.
set -euo pipefail
architecture="${MACOS_ARCH:-arm64}"
case "$architecture" in
    arm64|x86_64) ;;
    *) echo "Unsupported macOS architecture: $architecture" >&2; exit 1 ;;
esac
export PATH="$PWD/build/ci/tools/bin:$PATH"
export CCACHE_DIR="$PWD/build/ccache" CCACHE_BASEDIR="$PWD" CCACHE_COMPILERCHECK=content CCACHE_MAXSIZE=2G
trap 'ccache --show-stats || true' EXIT
llvm="$PWD/build/ci/tools/bin"
python3 tests/macos_tidy_test.py
cmake -S . -B build/cmake "-DPERIKOP_DEPENDENCY_DIR=$PWD/build/ci/sources" \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    "-DCMAKE_OSX_ARCHITECTURES=$architecture" "-DCMAKE_TEST_LAUNCHER=/usr/bin/arch;-$architecture" \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DwxBUILD_PRECOMP=OFF \
    "-DORTHO_CLANG_TIDY=$llvm/clang-tidy" "-DORTHO_RUN_CLANG_TIDY=$llvm/run-clang-tidy"
cmake --build build/cmake --parallel 3
python3 tools/check_macos_arch.py build/cmake/bin/Perikop.app --architecture "$architecture"
ctest --test-dir build/cmake --output-on-failure
cmake --build build/cmake --target tidy
python3 tools/check_resources.py
python3 tests/icon_test.py
python3 tests/linux_abi_test.py
python3 tests/ci_dependencies_test.py
python3 tests/macos_arch_test.py
python3 tests/update_packages_test.py
if [ "${PACKAGE:-false}" = true ]; then
    for attempt in 1 2 3; do
        cpack --config build/cmake/CPackConfig.cmake -B build/package && break
        [ "$attempt" = 3 ] && exit 1
        sleep 15
    done
    mount="$PWD/build/dmg-check"
    mkdir -p "$mount"
    hdiutil attach -readonly -nobrowse -mountpoint "$mount" build/package/Perikop-*-macOS-"$architecture".dmg
    trap 'hdiutil detach "$mount" || true; ccache --show-stats || true' EXIT
    python3 tools/check_macos_arch.py "$mount/Perikop.app" --architecture "$architecture"
    codesign --verify --deep --strict "$mount/Perikop.app"
    /usr/bin/arch "-$architecture" "$mount/Perikop.app/Contents/MacOS/Perikop" \
        --smoke-test --resources "$mount/Perikop.app/Contents/Resources"
    /usr/bin/arch "-$architecture" "$mount/Perikop.app/Contents/MacOS/Perikop" \
        --speech-probe "$PWD/build/voice-$architecture.wav" --resources "$mount/Perikop.app/Contents/Resources"
    hdiutil detach "$mount"
    trap 'ccache --show-stats || true' EXIT
fi
if [ "${PACKAGE:-false}" = true ] || [ "${GITHUB_EVENT_NAME:-}" = pull_request ]; then
    cmake --install build/cmake --prefix build/stage --strip
    bundle="$PWD/build/stage/Perikop.app"
    # Packing and the real update check run offline for both architectures, including PRs.
    PERIKOP_MACOS_TEST_SANDBOX=1 python3 tools/ci/velopack_macos_test.py "$bundle" "$architecture" \
        "build/macos-update-validation-$architecture"
    if [ "${PACKAGE:-false}" = true ]; then
        version=$(sed -n 's/^#define PERIKOP_VERSION "\([^"]*\)"/\1/p' build/cmake/generated/perikop_version.hpp)
        runtime=$architecture
        [ "$runtime" != x86_64 ] || runtime=x64
        track=releases
        [ "${NIGHTLY:-false}" != true ] || track=nightly
        channel="osx-$runtime-$track"
        output="build/velopack/$channel"
        python3 tools/ci/update_packages.py seed "build/update-base/$channel" "$output"
        python3 tools/package_velopack_macos.py "$bundle" "$version" "$architecture" \
            "$channel" "$output" --delta BestSpeed
        python3 tools/ci/update_packages.py finish "$output" "$channel" "$version" build/publish/updates
        mkdir -p build/publish/legacy
        cp build/package/Perikop-* build/publish/legacy/
    fi
fi
