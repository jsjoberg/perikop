# Build Perikop

## Requirements

- CMake 3.24 or later and a C++23 compiler with `std::expected`.
- Windows 10 or later, x86-64: w64devkit with MinGW-w64 GCC 13 or later.
- Linux x86-64: GCC 15 or later, GTK3 development headers, and Fontconfig development headers.
- macOS: Apple Clang 16 or later and the Xcode command-line tools.

On Linux, install the GTK3 and Fontconfig development packages for your distribution.
On Debian or Ubuntu, the package names are `libgtk-3-dev` and `libfontconfig1-dev`.
The compiler must support C++23. Portable speech requires macOS 13.4 or later.

Windows 10 is the minimum for the full application, including read-aloud.
The bundled [ONNX Runtime](https://onnxruntime.ai/docs/build/inferencing.html#target-environments)
and [Visual C++ runtime](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)
require Windows 10 or later.
Windows 7, 8, 8.1, and XP need a different speech runtime and separate validation.
CI builds on Windows Server 2025. Execution on Windows 10 still needs validation.

CI uses GCC 15 from the Ubuntu toolchain PPA on Ubuntu 24.04.
This keeps the AppImage's glibc baseline at 2.39.
On Ubuntu 24.04, install the compiler:

```sh
sudo add-apt-repository --yes ppa:ubuntu-toolchain-r/test
sudo apt-get update
sudo apt-get install -y g++-15
```

## Prepare read-aloud

The Alice and Björn voice pack occupies about 387 MB.
Prepare it before the build:

```sh
uv run --locked --group voice-prep tools/speech/prepare_kokoro.py --output build/kokoro-pack
```

The build copies `build/kokoro-pack` into the application.
Use `-DPERIKOP_VOICE_PACK=/path/to/pack` for another location.
Without a pack, CMake warns and builds Perikop without read-aloud.
The application shows a message for a missing or incomplete pack.

Use `uv` for all Python preparation commands.
The distributed application needs neither Python nor `uv`.
The [Kokoro voice guide](kokoro-voices.md) describes the models, export checks, and samples.

## Build

Run these commands from the repository directory:

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake --parallel
```

On Linux, select GCC 15 explicitly:

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=gcc-15 -DCMAKE_CXX_COMPILER=g++-15
cmake --build build/cmake --parallel
```

The first build downloads pinned wxWidgets, SQLite, and portable speech dependencies.
CMake checks their SHA-256 hashes. ONNX Runtime ships as a shared library beside the application.
The remaining libraries build statically.
The application has no runtime scripting dependency.

See [the architecture guide](architecture.md) for code boundaries and formatting commands.

If you use w64devkit, run `build.cmd` in its shell.
The script selects the MinGW Makefiles generator.

For a development build with installed wxWidgets, add `-DORTHO_SYSTEM_WX=ON`.
The default build uses the pinned static library.

## Reuse dependency sources

If dependency archives are already available, supply their source directories:

```sh
cmake -S . -B build/cmake \
  -DFETCHCONTENT_SOURCE_DIR_SQLITE=/absolute/path/sqlite-amalgamation-3530400 \
  -DFETCHCONTENT_SOURCE_DIR_WXWIDGETS=/absolute/path/wxWidgets-3.3.3
```

These options reuse SQLite and wxWidgets sources.
An offline build also needs the other pinned dependencies locally available.

## Run

On macOS, open `build/cmake/bin/Perikop.app`.
On Windows or Linux, run `build/cmake/bin/perikop`.
The executable uses resources beside it or inside the macOS application bundle.

For Psalm 23, run:

```sh
"build/cmake/bin/Perikop.app/Contents/MacOS/Perikop" --reader --date 2026-10-05
```

On Windows or Linux, use the platform executable with the same arguments.

See the [user guide](using-perikop.md), [tests](testing.md), and [release guide](releasing.md) for the next steps.
