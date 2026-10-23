# Build Perikop

## Requirements

- CMake 3.24 or later and a C++23 compiler with `std::expected`.
- Windows: w64devkit with MinGW-w64 GCC 15 or later.
- Linux: GCC 15 or later and the GTK3 and Fontconfig development headers (`libgtk-3-dev libfontconfig1-dev` on Debian and Ubuntu).
  On RHEL or Rocky Linux 9: `sudo dnf install gcc-toolset-15-gcc gcc-toolset-15-gcc-c++ gtk3-devel fontconfig-devel cmake make`, then `scl enable gcc-toolset-15 bash`.
- macOS: Apple Silicon, macOS 15 or later, Apple Clang 16 or later, and the Xcode command-line tools.
- Python tooling: uv; `.python-version` pins Python 3.14.6. `uv run --locked` prepares it automatically.

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
- macOS builds target `arm64` only. Check the bundle with `uv run --locked tools/check_macos_arch.py build/cmake/bin/Perikop.app --architecture arm64`.

CI builds Apple Silicon on macOS 15. Intel and universal Mac builds are rejected. macOS compilation, signing, and packaging stay native; there is no macOS cross-compilation migration to maintain.

The shared `tools/ci/build.py` entry point runs `linux`, `macos`, `windows` (cross-build), `sanitizers` (macOS), or `linux-sanitizers`. `.github/actions/python` installs uv 0.13.1 and the pinned Python before any network-denied build. Host checks use that interpreter; Linux containers mount its relocatable installation read-only. Its bundled SQLite reads STRICT tables without compiling or injecting a replacement library. This tooling bootstrap can download from upstream; application dependencies still come only from verified snapshots, and compilation/packaging run offline.

To build offline from the same dependency snapshot as CI (`Linux`, `macOS`, or `Windows`):

```sh
python3 tools/ci/dependencies.py restore --platform macOS
cmake -S . -B build/offline -DPERIKOP_DEPENDENCY_DIR="$PWD/build/ci/sources"
```

## Windows cross-builds from Linux

The Windows x64 build uses the pinned llvm-mingw environment on Linux. CI has one compilation path; the native Windows compilation fallback has been removed. Local Windows builds still support w64devkit.

Linux transfers the binaries and assembled resources to Windows.
Linux also exports the CTest commands and strips the application executable.
Windows configures local test paths and installation with a compiler-free CMake project.
Windows still runs native tests and packages the installed files with Velopack.

Windows restores only Velopack's packaging tool from the dependency snapshot.
Windows does not extract the source dependencies. Snapshots contain no native Windows compiler or compiler cache.
Local Windows builds use an installed w64devkit. `dependencies.py restore --platform Windows` restores sources and packaging tools for an offline native build, without installing a compiler.

With llvm-mingw on the Linux `PATH`, the same offline cross-build is:

```sh
python3 tools/ci/dependencies.py restore --platform Windows --cross
cmake -S . -B build/win-x64 -G Ninja --toolchain cmake/llvm-mingw.cmake \
    -DPERIKOP_DEPENDENCY_DIR="$PWD/build/ci/sources" -DwxBUILD_PRECOMP=OFF
cmake --build build/win-x64 --parallel
```

The restore command also downloads the pinned environment archive for container builds.
Windows ARM64 CI is not part of this migration step.

## Read-aloud

Read-aloud uses the user's own eSpeak NG installation, loaded at run time; Perikop bundles only its Swedish pronunciation data in `resources/speech`. No preparation step is needed. Tests that synthesize speech run where eSpeak NG is installed (for example `brew install espeak-ng`). Missing libraries are reported as skipped locally; failures to initialize an installed library fail the test. Native macOS CI installs eSpeak NG and sets `PERIKOP_REQUIRE_ESPEAK=1`, so a missing library also fails that job.

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

CI runs `tidy` in the Linux x64 job with Rocky Linux 9's clang-tidy 21. The macOS job analyzes only the sources that Linux does not compile in the same way: `.mm` files and `.cpp` files with `__APPLE__` or `__WXOSX__` sections. It uses the pinned clang-tidy 23 and sets `-DORTHO_TIDY_FILES`, a regular expression relative to the source tree.

## Sanitizers

`-DORTHO_SANITIZERS=ON` builds Perikop's code with AddressSanitizer and UndefinedBehaviorSanitizer. CI runs the full CTest suite with them in two jobs:

- `sanitizers` on macOS 15 with Apple Clang.
- `linux-sanitizers` in the Rocky Linux container with GCC 15, under Xvfb. On Linux, AddressSanitizer also enables LeakSanitizer. `ci/lsan.supp` suppresses process-lifetime allocations in fontconfig and librsvg, and `NO_AT_BRIDGE=1` keeps GTK from opening an accessibility bus connection. Do not add suppressions for Perikop's own code.

Both sanitizer jobs cache compiler output separately from release builds. Cache keys distinguish Linux x64, Linux ARM64, macOS ARM64, and sanitizer builds; all tests still run on every build. Each job reports its own cache hits and misses, including sanitizer jobs. Compilation uses the CPUs available to the process (three on the standard macOS runner, four on public Linux runners).
