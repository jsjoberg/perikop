# Orthodox Reader

A native Orthodox lectionary reader prototype for Windows, macOS, and Linux.
The interface uses Swedish labels. Scripture appears in a custom native view with continuous chapter context.

Version 0.5 bundles 141,201 Scripture records from complete available Swedish, Greek, and English editions.
It includes Swedish apocrypha and the Greek Septuagint.
It calculates daily readings offline for the North American Antiochian Greek tradition.
All 52 Sundays match the Archdiocese's official 2026 chart.

The reader scrolls in pixels and preserves macOS trackpad precision and momentum.
Native paragraph fitting uses Knuth–Plass demerits, Liang hyphenation, and fractional native glyph measurements.
Prose flows across verses. Small inline numbers preserve verse navigation.
The blue and yellow Orthodox cross appears in the window and the macOS Dock.
It bundles Literata, IBM Plex Sans, and fallback fonts for Hebrew headings and editorial brackets.
The application needs no network connection after installation.

## Build

Requirements:

- CMake 3.24 or later and a C++23 compiler with `std::expected`.
- Windows 11 x86-64: w64devkit with MinGW-w64 GCC 13 or later.
- Linux x86-64: GCC 13 or later, GTK3 development headers, and Fontconfig development headers.
- macOS: Apple Clang 16 or later and the Xcode command-line tools.

On Linux, install the GTK3 and Fontconfig development packages for your distribution.
On Debian or Ubuntu, the package names are `libgtk-3-dev` and `libfontconfig1-dev`.
The compiler must support C++23. Portable speech requires macOS 13.4 or later.

Run these commands from the repository directory:

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake --parallel
```

The first build downloads pinned wxWidgets, SQLite, and portable speech dependencies.
CMake checks their SHA-256 hashes. ONNX Runtime ships as a shared library beside the application.
The remaining libraries build statically.
The application has no runtime scripting dependency.

If you use w64devkit, run `build.cmd` in its shell.
The script selects the MinGW Makefiles generator.

If dependency archives are already available, supply their source directories for an offline build:

```sh
cmake -S . -B build/cmake \
  -DFETCHCONTENT_SOURCE_DIR_SQLITE=/absolute/path/sqlite-amalgamation-3530400 \
  -DFETCHCONTENT_SOURCE_DIR_WXWIDGETS=/absolute/path/wxWidgets-3.3.3
```

For a development build with installed wxWidgets, add `-DORTHO_SYSTEM_WX=ON`.
The default build uses the pinned static library.

## Run

On macOS, open `build/cmake/bin/Orthodox Reader.app`.
On Windows or Linux, run `build/cmake/bin/orthodox-reader`.
The executable uses resources beside the executable or inside the macOS application bundle.

For Psalm 23, run:

```sh
"build/cmake/bin/Orthodox Reader.app/Contents/MacOS/Orthodox Reader" --reader --date 2026-10-05
```

On Windows or Linux, use the platform executable with the same arguments.

The application selects the current local civil date at startup.
Only explicit actions change this date. Midnight, sleep, and theme changes do not change it.

Select a reading to open its chapter context.
Use the mouse wheel, arrow keys, Page Up, Page Down, Home, or End to move through the text.
The brass margin line marks the selected passage.
The text control adds Greek, English, or all three languages to the selected main edition.
The Bible action opens a native book, chapter, verse, and edition selector.
Narrow windows use aligned blocks.

Theme choices are System, Light, and Dark.
The user database stores the theme, calendar choice, parallel language, and font size.
The selected date does not persist between launches.

The Calendar action opens a native date picker. Its Current date button selects the current date before confirmation.
New calendar mode uses the North American Antiochian reading rules.
Old calendar mode applies those Greek rules to Julian fixed dates. It is a comparison mode.
Julian conversion calculates the date difference for each century.

Lyssna uses a local Chatterbox Multilingual model through common C++ code on all target platforms.
The same model, reference voice, and settings apply everywhere. No system voice is used.
The voice remains a preview pending Swedish listening review and remaining platform checks.
Pausa, Fortsätt, and Stoppa control playback. A status line shows preparation progress and identifies the voice.
The engine prepares the entire requested reading before playback. First-time synthesis can take longer than the resulting audio.
A separate SQLite cache accelerates repeat readings. The voice pack needs about 1.55 GB.
See [the speech selection record](docs/speech-selection.md) for licensing, alternatives, measurements, and limits.

Install the pinned voice package once:

```sh
python3 tools/speech/install_voice.py
python3 tools/speech/install_voice.py --verify
```

For an unpacked offline voice package, use `--source /absolute/path/voice-pack`.
Python performs installation only. The application needs no interpreter or network connection for playback.
The pack remains outside application upgrades in the platform user-data directory.
Missing or incomplete packs produce a visible message.
Display text and speech text remain separate.
The pronunciation example changes `Melkisedek` to `Melki-sedek` for speech only.

## Tests

Run the core, storage, speech, and GUI smoke tests:

```sh
ctest --test-dir build/cmake --output-on-failure
```

The GUI smoke test needs a graphical desktop session.
On Linux, use Xvfb if no desktop session is available:

```sh
xvfb-run -a ctest --test-dir build/cmake --output-on-failure
```

For core tests without a GUI, use `-DORTHO_BUILD_GUI=OFF` in a separate build directory.

The GUI smoke test loads the fonts and draws each theme and parallel mode.
It uses a temporary user database. It does not change personal settings.

To save reader images during the smoke test, add `--screenshot /absolute/path/reader.png`.
The paragraph test compares 39 native layouts with an exhaustive word-boundary oracle and checks a layout that differs from greedy wrapping.

Verify real offline Swedish audio generation without playing it:

```sh
"build/cmake/bin/Orthodox Reader.app/Contents/MacOS/Orthodox Reader" --speech-probe /absolute/path/voice.wav
```

Use the corresponding executable on Windows or Linux.
This test requires the pinned local voice pack. It rejects token-limit truncation, invalid samples, and silent audio.

## Offline corpus

`resources/corpus/corpus.db` opens in read-only mode.
`user.db` resides in the platform user-data directory.
All SQL stays in the storage layer. See `docs/storage.md` for the explicit SQLite policy and migration.

The corpus contains 35,515 Swedish records, 28,597 Greek OT records, 7,958 Greek NT records, 31,102 KJV records, and 38,029 WEB records.
The World English Bible includes deuterocanonical books. The Bible browser offers it as a separate edition.
Lettered Greek coordinates, such as Genesis 31:50a, remain separate records. Joined publisher verses retain their complete ranges.
The browser lists only books available in the selected edition.
Each source retains its wording and chapter and verse coordinates.
The Greek Psalms retain LXX numbering.
An explicit alignment links Swedish, Greek, and English verses across their numbering systems.
Verses without a counterpart show a message. See `docs/versification.md`.
The renderer retains book coordinates and at most 192 text layouts.
It creates no native control for individual verses.

Rebuild the corpus from bundled source data:

```sh
python3 tools/corpus/build_corpus.py
```

Python is a corpus development tool. Normal builds and application startup do not need Python.
`resources/manifest.json` records resource hashes and provenance.
`THIRD-PARTY-NOTICES.md` records licenses and edition limits.

Check the resource hashes and complete corpus glyph coverage:

```sh
python3 tools/check_resources.py
```

## Current limits

Prose uses bundled USFM paragraph boundaries. Swedish and KJV use WEB boundaries as editorial display metadata where coordinates agree.
Poetry retains verse stanzas. Paragraph breaks inside a single verse are not yet retained.
Punctuation protrudes into optical margins.
Word-level MT/LXX alignment and morning and evening Psalm cycles remain open.
Swedish voice quality and Windows/Linux/Intel Mac speech execution still need independent validation.
Published annual Antiochian instructions can require additional calendar exceptions.
No restricted modern Swedish translation is bundled.

See `docs/lectionary.md` for the calculation rules and reference sources.
See `docs/engineering-spec.md` for the original handoff specification.
See `docs/validation.md` for local results and outstanding platform checks.
