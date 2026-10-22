#!/usr/bin/env python3
"""Build and check one target with prepared dependencies and networking disabled."""

import argparse
import os
from pathlib import Path
import platform
import re
import subprocess
import sys

ROOT = Path.cwd()
BUILD = Path("build/cmake")
SOURCES = ROOT / "build/ci/sources"
DISPLAY = ["xvfb-run", "-a", "-s", "-screen 0 1600x1200x24"]


def run(*arguments, **kwargs):
    return subprocess.run([str(argument) for argument in arguments], check=True, **kwargs)


def python(script, *arguments):
    run(sys.executable, script, *arguments)


def build(*options, directory=BUILD, jobs=3, cache=True):
    arguments = ["cmake", "-S", ".", "-B", directory, f"-DPERIKOP_DEPENDENCY_DIR={SOURCES}",
                 "-DwxBUILD_PRECOMP=OFF", *options]
    if cache:
        arguments += ["-DCMAKE_C_COMPILER_LAUNCHER=ccache", "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache"]
    run(*arguments)
    run("cmake", "--build", directory, "--parallel", jobs)


def checks(*names):
    for name in names:
        python(f"tests/{name}_test.py")


def version():
    return re.search(r'^#define PERIKOP_VERSION "([^"]+)"',
                     (BUILD / "generated/perikop_version.hpp").read_text(), re.MULTILINE)[1]


def publish_updates(target, pack):
    channel = target + ("-nightly" if os.environ.get("NIGHTLY") == "true" else "-releases")
    output = Path("build/velopack") / channel
    current = version()
    python("tools/ci/update_packages.py", "seed", Path("build/update-base") / channel, output)
    pack(current, channel, output)
    python("tools/ci/update_packages.py", "finish", output, channel, current, "build/publish/updates")


def apple_sources():
    # Linux analyzes the shared code; macOS analyzes only sources with Apple-specific code.
    sources = sorted(path for folder in ("src", "tests", "tools") for path in (ROOT / folder).rglob("*"))
    return [path.relative_to(ROOT).as_posix() for path in sources if path.suffix == ".mm"
            or path.suffix == ".cpp" and re.search(r"__APPLE__|__WXOSX__", path.read_text())]


def linux(package):
    # The pinned toolchain image remains compatible with the glibc 2.34 app baseline.
    run("git", "config", "--global", "--add", "safe.directory", ROOT)
    os.environ.update(PERIKOP_OFFLINE="1", PERIKOP_APPIMAGE_TOOLS=str(ROOT / "build/ci/tools/appimage"))
    build("-DCMAKE_BUILD_TYPE=MinSizeRel")
    python("tools/check_linux_abi.py", BUILD / "bin")
    try:
        run(*DISPLAY, "ctest", "--test-dir", BUILD, "--output-on-failure")
    except subprocess.CalledProcessError:
        Path("build/smoke").mkdir(exist_ok=True)
        subprocess.run([*DISPLAY, "gdb", "-batch", "-ex", "run", "-ex", "bt", "--args",
                        str(BUILD / "bin/perikop"), "--smoke-test", "--resources", str(BUILD / "bin/resources"),
                        "--screenshot", str(ROOT / "build/smoke/reader.png")], check=False)
        raise
    # One architecture is enough for analysis; the code is the same.
    if platform.machine() == "x86_64":
        run("cmake", "--build", BUILD, "--target", "tidy")
    # uv's standalone Python carries modern SQLite; no LD_PRELOAD or custom SQLite build is needed.
    python("tools/check_resources.py")
    checks("osb_metadata", "modern_lexicon", "icon", "linux_abi", "ci_dependencies", "ci_build", "windows_cross",
           "appimage_licenses", "nightly", "update_packages")
    if package:
        run("tools/prepare_appdir.sh", BUILD)
        python("tools/ci/velopack_linux_test.py", BUILD)
        runtime = {"x86_64": "x64", "aarch64": "arm64"}[platform.machine()]
        os.environ["PERIKOP_VELOPACK_DELTA"] = "BestSpeed"
        publish_updates("linux-" + runtime, lambda current, channel, output:
                        run("tools/package_velopack.sh", BUILD, current, channel, output))
        python("tools/appimage_runtime.py", "check", BUILD)
        python("tools/appimage_runtime.py", "source", "--output", "build/publish/updates")


def macos(package):
    os.environ["PATH"] = str(ROOT / "build/ci/tools/bin") + os.pathsep + os.environ["PATH"]
    checks("macos_tidy")
    build("-DCMAKE_BUILD_TYPE=MinSizeRel", "-DCMAKE_OSX_ARCHITECTURES=arm64",
          "-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0",
          f"-DORTHO_CLANG_TIDY={ROOT}/build/ci/tools/bin/clang-tidy",
          f"-DORTHO_RUN_CLANG_TIDY={ROOT}/build/ci/tools/bin/run-clang-tidy",
          f"-DORTHO_TIDY_FILES=({'|'.join(map(re.escape, apple_sources()))})$")
    python("tools/check_macos_arch.py", BUILD / "bin/Perikop.app", "--architecture", "arm64")
    run("ctest", "--test-dir", BUILD, "--output-on-failure")
    run("cmake", "--build", BUILD, "--target", "tidy")
    python("tools/check_resources.py")
    checks("osb_metadata", "icon", "linux_abi", "ci_dependencies", "ci_build", "macos_arch", "nightly", "update_packages")
    if package or os.environ.get("GITHUB_EVENT_NAME") == "pull_request":
        run("cmake", "--install", BUILD, "--prefix", "build/stage", "--strip")
        bundle = ROOT / "build/stage/Perikop.app"
        if package:
            # Velopack packs this bundle: check what releases ship.
            python("tools/check_macos_arch.py", bundle, "--architecture", "arm64")
            run("codesign", "--verify", "--deep", "--strict", bundle)
            run(bundle / "Contents/MacOS/Perikop", "--smoke-test", "--resources", bundle / "Contents/Resources")
        os.environ["PERIKOP_MACOS_TEST_SANDBOX"] = "1"
        python("tools/ci/velopack_macos_test.py", bundle, "arm64", "build/macos-update-validation-arm64")
        if package:
            publish_updates("osx-arm64", lambda current, channel, output:
                            python("tools/package_velopack_macos.py", bundle, current, "arm64",
                                   channel, output, "--delta", "BestSpeed"))


def windows():
    build("-G", "Ninja", "--toolchain", "cmake/llvm-mingw.cmake", "-DCMAKE_BUILD_TYPE=MinSizeRel", jobs=4)
    python("tools/ci/windows_handoff.py", "--source", ".", "--build", BUILD)
    run("llvm-strip", BUILD / "bin/perikop.exe")
    # Transfer binaries and test metadata, without host-specific CMake state.
    run("tar", "-czf", "build/windows-x64.tar.gz", "-C", BUILD,
        "bin", "generated/perikop_version.hpp", "windows-handoff.json",
        env=dict(os.environ, COPYFILE_DISABLE="1"))


def sanitizers(*options, display=()):
    os.environ.update(UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1", ASAN_OPTIONS="halt_on_error=1")
    directory = Path("build/sanitizers")
    build("-DCMAKE_BUILD_TYPE=Debug", "-DORTHO_SANITIZERS=ON", *options, directory=directory, cache=False)
    run(*display, "ctest", "--test-dir", directory, "--output-on-failure", "--timeout", "240")


def linux_sanitizers():
    run("git", "config", "--global", "--add", "safe.directory", ROOT)
    # LeakSanitizer is part of AddressSanitizer on Linux.
    os.environ["LSAN_OPTIONS"] = f"suppressions={ROOT / 'ci/lsan.supp'}"
    sanitizers(display=DISPLAY)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target", choices=("linux", "macos", "windows", "sanitizers", "linux-sanitizers"))
    args = parser.parse_args()
    os.environ.update(CCACHE_DIR=str(ROOT / "build/ccache"), CCACHE_BASEDIR=str(ROOT),
                      CCACHE_COMPILERCHECK="content", CCACHE_MAXSIZE="2G")
    try:
        package = os.environ.get("PACKAGE") == "true"
        commands = {"linux": lambda: linux(package), "macos": lambda: macos(package),
                    "windows": windows,
                    "sanitizers": lambda: sanitizers("-DCMAKE_OSX_ARCHITECTURES=arm64",
                                                     "-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0"),
                    "linux-sanitizers": linux_sanitizers}
        commands[args.target]()
    finally:
        if not args.target.endswith("sanitizers"):
            subprocess.run(["ccache", "--show-stats"], check=False)
