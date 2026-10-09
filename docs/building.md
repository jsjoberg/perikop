# Build Perikop

## Requirements

- CMake 3.24 or later and a C++23 compiler with `std::expected`.
- Windows 10 or later, x86-64: w64devkit with MinGW-w64 GCC 15 or later.
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

CI uses stable GCC 15 on both Windows and Linux.
Windows CI uses [w64devkit 2.7.0](https://github.com/skeeto/w64devkit/releases/tag/v2.7.0), which contains GCC 15.2.0.
Linux CI uses the distribution's GCC Toolset 15 inside Rocky Linux 9.
The packaged app targets glibc 2.34, which includes enterprise Linux 9 and Ubuntu 22.04 or later.
Desktop and read-aloud compatibility on these systems still need validation.

On RHEL 9 or Rocky Linux 9, install and enable the compiler:

```sh
sudo dnf install gcc-toolset-15-gcc gcc-toolset-15-gcc-c++ gtk3-devel fontconfig-devel cmake make
scl enable gcc-toolset-15 bash
```

The toolset leaves the system compiler and glibc in place.
On Debian or Ubuntu, install a stable GCC 15 package from a trusted repository for your distribution.

## Build

Run these commands from the repository directory:

```sh
cmake -S . -B build/cmake
cmake --build build/cmake --parallel
```

CMake defaults to Release for generators with one build configuration.
On Linux, CMake selects the installed `gcc-15` and `g++-15` pair.
Explicit compiler options, `CC`, `CXX`, and toolchain files take precedence.
For a debug build, add `-DCMAKE_BUILD_TYPE=Debug`.

The first build downloads pinned wxWidgets, SQLite, and portable speech dependencies.
CMake checks their SHA-256 hashes. ONNX Runtime ships as a shared library beside the application.
Linux GNU builds also carry the selected compiler's `libstdc++` and `libgcc_s` beside the application.
The remaining libraries build statically.
The application has no runtime scripting dependency.

See [the architecture guide](architecture.md) for code boundaries and formatting commands.

On Windows, run `build.cmd` in the w64devkit shell.
The script selects the MinGW Makefiles generator and runs both build commands.
On macOS or Linux, `./build.sh` runs both commands.

For a development build with installed wxWidgets, add `-DORTHO_SYSTEM_WX=ON`.
The default build uses the pinned static library.

The application builds without Python, Swift, or a voice pack.
CI supplies extra options for compiler caching, static analysis, and release packaging.
These options are optional for local builds.

## Optional read-aloud

The Alice and Björn voice pack occupies about 180 MB.
To include read-aloud, prepare the pack before the build:

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

## Update the icon

The repository includes the SVG artwork, PNG sizes, Windows ICO, and macOS ICNS.
Normal builds use these files directly.

After an artwork change, export the PNG sizes into `resources/icons/OrthodoxReader.iconset`.
Export the 1024-pixel PNG as `resources/icons/orthodox-cross.png` for the embedded application icon.
Then package the native icons:

```sh
uv run --locked tools/make_icon.py
```

The tool uses only Python's standard library and runs on all three platforms.
It preserves the PNG artwork, including the 48-pixel Windows icon.
For the default output directory, it also updates the icon hashes in `resources/manifest.json`.

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
