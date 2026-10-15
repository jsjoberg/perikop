# Build Perikop

## Requirements

- CMake 3.24 or later and a C++23 compiler with `std::expected`.
- Windows: w64devkit with MinGW-w64 GCC 15 or later.
- Linux: GCC 15 or later and the GTK3 and Fontconfig development headers (`libgtk-3-dev libfontconfig1-dev` on Debian and Ubuntu).
  On RHEL or Rocky Linux 9: `sudo dnf install gcc-toolset-15-gcc gcc-toolset-15-gcc-c++ gtk3-devel fontconfig-devel cmake make`, then `scl enable gcc-toolset-15 bash`.
- macOS: Apple Clang 16 or later and the Xcode command-line tools.

## Build

```sh
cmake -S . -B build/cmake
cmake --build build/cmake --parallel
```

`./build.sh` (macOS, Linux) and `build.cmd` (w64devkit shell) run both commands.
The first build downloads the pinned dependencies and checks their SHA-256 hashes.
The default build type is MinSizeRel with link-time optimization. Useful options:

- `-DCMAKE_BUILD_TYPE=Debug`
- `-DPERIKOP_LTO=OFF`
- `-DORTHO_SYSTEM_WX=ON` to use an installed wxWidgets.
- `'-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'` for a universal macOS app. Check it with `python3 tools/check_macos_arch.py build/universal/bin/Perikop.app`.

To build offline from the same dependency snapshot as CI (`Linux`, `macOS`, or `Windows`):

```sh
python3 tools/ci/dependencies.py restore --platform macOS
cmake -S . -B build/offline -DPERIKOP_DEPENDENCY_DIR="$PWD/build/ci/sources"
```

## Windows cross-builds from Linux

The Windows x64 build uses llvm-mingw on Linux after the dependency snapshot includes `Windows-cross-environment.tar.gz`.
Until then, CI compiles on Windows.
The manual dependency workflow prepares this environment with llvm-mingw 20260922 and checks the archive hash.
To activate Linux compilation, run that workflow and commit its `dependencies-lock.json` as `ci/dependencies-lock.json`.

Linux transfers the binaries and assembled resources to Windows.
Windows configures local test paths and the installer, but does not recompile these binaries.
Windows still runs native tests and the speech probe, supplies the MSVC runtime, and creates the installer.
ONNX Runtime and CRT distribution remain unchanged.
The Windows configuration still needs w64devkit during this transition.
Local Windows builds continue to use w64devkit.

With llvm-mingw on the Linux `PATH`, the same offline cross-build is:

```sh
python3 tools/ci/dependencies.py restore --platform Windows --cross
cmake -S . -B build/win-x64 -G Ninja --toolchain cmake/llvm-mingw.cmake \
    -DPERIKOP_DEPENDENCY_DIR="$PWD/build/ci/sources" -DwxBUILD_PRECOMP=OFF
cmake --build build/win-x64 --parallel
```

The restore command also downloads the pinned environment archive for container builds.
Windows ARM64 CI is not part of this migration step.

## Read-aloud voices

Without a voice pack, Perikop builds without read-aloud. Prepare the pack (about 180 MB) before the build:

```sh
uv run --locked --group voice-prep tools/speech/prepare_kokoro.py --output build/kokoro-pack
```

The build copies `build/kokoro-pack` into the app. Use `-DPERIKOP_VOICE_PACK=/path/to/pack` for another location.

## Run

On macOS, open `build/cmake/bin/Perikop.app`. On Windows or Linux, run `build/cmake/bin/perikop`.
Add `--profile /absolute/path` for a separate profile, or `--reader --date 2026-10-05` to open a day's reading.

## Resources

Rebuild the corpus after a change to `resources/corpus` or the tools. After an alignment change, run `tools/corpus/align_editions.py` first.

```sh
uv run --locked tools/corpus/build_corpus.py
uv run --locked tools/check_resources.py
```

After an icon change, export the PNG sizes into `resources/icons/OrthodoxReader.iconset` and the 1024-pixel PNG as `resources/icons/orthodox-cross.png`, then run:

```sh
uv run --locked tools/make_icon.py
```

## Format and lint

With `clang-format` and `clang-tidy` on the `PATH` when CMake configures:

```sh
cmake --build build/cmake --target format
cmake --build build/cmake --target format-check
cmake --build build/cmake --target tidy
```

Any `tidy` finding is an error. To keep a wrong finding, add `// NOLINT(check-name)` with the reason.
