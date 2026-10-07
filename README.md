# Perikop

A native Orthodox lectionary reader prototype for Windows, macOS, and Linux.
The interface uses Swedish labels. Scripture appears in a custom native view with continuous chapter context.

Perikop bundles 141,201 Scripture records from complete available Swedish, Greek, and English editions.
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

See [the architecture guide](docs/architecture.md) for code boundaries and formatting commands.

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

## Release packages

Each package contains the application, its resources, and the Alice and Björn voices. It works offline.

- macOS: `Perikop-0.1.0-macOS-arm64.dmg`. Drag Perikop to Applications.
- Windows: `Perikop-0.1.0-windows-x64.exe`. The installer puts Perikop in Program Files and adds a Start menu shortcut.
- Linux: `Perikop-0.1.0-x86_64.AppImage`. Make the file executable, then run it.

Prepare the voice pack and build Perikop first. Then, on macOS or Windows, make the package:

```sh
cpack --config build/cmake/CPackConfig.cmake -B build/package
```

On Windows, the installer needs NSIS on the `PATH`.
ONNX Runtime needs the Visual C++ runtime, so the installer carries it.
Configure with `-DPERIKOP_MSVC_RUNTIME` set to the `Microsoft.VC143.CRT` folder of a Visual Studio redistributable, for example `VC\Redist\MSVC\<version>\x64\Microsoft.VC143.CRT`.

On Linux, make the AppImage:

```sh
tools/package_appimage.sh build/cmake
```

The script downloads pinned linuxdeploy tools and checks their SHA-256 hashes. They copy GTK and the other shared libraries into the image.
The AppImage runs on distributions with the glibc of its build system or later. Ubuntu 24.04 needs glibc 2.39.

The packages go to `build/package`.
The macOS application has an ad-hoc signature only. On first launch, open **System Settings → Privacy & Security** and select **Open Anyway**.
The Windows installer has no signature, so SmartScreen shows a warning.
CI makes the packages for version tags `v*` and for manual runs.

## Run

On macOS, open `build/cmake/bin/Perikop.app`.
On Windows or Linux, run `build/cmake/bin/perikop`.
The executable uses resources beside the executable or inside the macOS application bundle.

For Psalm 23, run:

```sh
"build/cmake/bin/Perikop.app/Contents/MacOS/Perikop" --reader --date 2026-10-05
```

On Windows or Linux, use the platform executable with the same arguments.

The application selects the current local civil date at startup.
Only explicit actions change this date. Midnight, sleep, and theme changes do not change it.

Select a reading to open its chapter context.
Use the mouse wheel, arrow keys, Page Up, Page Down, Home, or End to move through the text.
The brass margin line marks the selected passage.
The text control adds Greek, English, or all three languages to the selected main edition.
The Bible action opens a native book, chapter, verse, and edition selector.
Books outside the Hebrew Bible are marked with an asterisk.
Books without text in the left pane's language are greyed out, and a note under the grid names them.
**Hela boken** and **Hela kapitlet**, before the numbers, open a whole book or chapter. Lyssna then reads all of it.

**Visa → Höger spalt → Ordstudium** makes the right pane a lookup panel. Click a word to see its pronunciation, Dalin's 1850 definition and, in the New Testament, the Strong's entries of the verse.
See [the word-study guide](docs/word-study.md) for the data and its limits.
Narrow windows use aligned blocks.

Theme choices are System, Light, and Dark.
The user database stores the theme, calendar choice, parallel language, and font size.
The selected date does not persist between launches.

A browser-like toolbar sits at the top of every page, with four symbols on each side of the address field.
From the left: Back returns to the day page, then Lyssna, Pausa, and Stoppa control read-aloud. Symbols that do not apply are dimmed.
Click the address field to go to another passage. For a reading in several parts, the field is a menu of the parts.
The symbols on the right choose the right column: word study, Swedish, Greek, or English. Click the chosen symbol again to close the column.
On the day page, the reader's controls are dimmed.
**Om Perikop** shows the version and every source the program uses, with links. On macOS it is in the application menu; elsewhere it is in **Hjälp**.

The day page shows the date, its readings, and three reading plans. The **Kalender** menu moves between days and opens a native date picker.

The reading plans are the New Testament in 30 parts, the Masoretic canon in 30 parts, and more books from the Septuagint in 14 parts.
The last plan contains the books outside the Hebrew Bible that Perikop has in Swedish. First Esdras, Third Maccabees, and Psalm 151 are not in it, because Perikop has no Swedish text for them.
Plans count parts, not days. Select a numbered part to open it; a part with several books has a section menu in the address field.
When the voice finishes a part, or you leave a part after you scroll to its end, Perikop asks whether to mark it as read.
The day's readings are marked in the same way. **Börja om** clears one plan after confirmation.
The Masoretic-canon plan keeps the Hebrew Bible's book order. Its parts have about the same length and use the Septuagint's chapter numbers.
The picker's Current date button selects the current date before confirmation.
The build patches a wxWidgets 3.3.3 macOS bug that over-released Swedish month and weekday names and crashed the date picker.
New calendar mode uses the North American Antiochian reading rules.
Old calendar mode applies those Greek rules to Julian fixed dates. It is a comparison mode.
Julian conversion calculates the date difference for each century.

Lyssna reads Swedish with the Alice or Björn voice from the local Swedish Kokoro model.
Select the voice in **Uppläsning → Alice** or **Björn**. The choice applies from the next reading and survives restarts.
Read-aloud is Swedish only. With Greek or English in the left pane, Lyssna is disabled.
The voice runs through common C++ code on all target platforms. No system voice is used.
The voices remain a preview pending listening review and remaining platform checks.

Pausa, Fortsätt, and Stoppa control playback. The address field shows loading, buffering, the introduction as it is spoken, and the current verse.
A line along its lower edge shows progress.
Playback starts after the first audio chunk. The engine generates later chunks during playback, with a buffer of at most two chunks.
Initial model loading and first-chunk synthesis still take time. If synthesis cannot keep pace, playback waits for the next chunk.

Lyssna opens the passage and follows the spoken text with a soft highlight and a moving margin marker.
**Visa → Markera texten som läses upp** turns the highlight off; the margin marker stays, and the text still follows the voice.
Manual scrolling releases automatic following. A translucent Följ uppläsningen button then floats above the text and returns to the current text.
Without playback, Till läsningen appears in the same place when the passage is scrolled out of view.
Pause freezes the marker and scrolling. Stop clears the marker and keeps the page position.

Verse boundaries follow audio playback. Movement between lines within a verse is an estimate, because the model supplies no word timestamps.

A separate SQLite cache accelerates repeat readings.
See [the speech selection record](docs/speech-selection.md) for licensing, alternatives, measurements, and limits.

The Alice and Björn voice pack, about 387 MB, is bundled in the application. Perikop never uses the network.
Prepare the pack before you build, as [the Kokoro voice guide](docs/kokoro-voices.md) describes:

```sh
uv run --locked --group voice-prep tools/speech/prepare_kokoro.py --output build/kokoro-pack
```

The build copies `build/kokoro-pack` into the application. Use `-DPERIKOP_VOICE_PACK=/path/to/pack` for another location.
Without a pack, CMake warns and builds Perikop without read-aloud.
Use `uv` for all Python preparation commands. The distributed application needs neither Python nor `uv`.
Missing or incomplete packs produce a visible message.
Display text and speech text remain separate.
The pronunciation example changes `Melkisedek` to `Melki-sedek` for speech only.

Use **Uppläsning → Granska svenskt uttal…** to review the prepared Swedish word list.
The tool offers word and verse previews, local corrections, review decisions, and TSV export.
Saved corrections apply to subsequent Swedish playback.
See [the pronunciation review guide](docs/pronunciation-review.md) for the workflow and data limits.

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

Verify real offline Swedish audio generation with Alice without playing it:

```sh
"build/cmake/bin/Perikop.app/Contents/MacOS/Perikop" --speech-probe /absolute/path/voice.wav
```

Use the corresponding executable on Windows or Linux.
This test requires the pinned local voice pack. It rejects token-limit truncation, invalid samples, and silent audio.

## Offline corpus

`resources/corpus/corpus.db` opens in read-only mode.
`user.db` resides in the platform user-data directory.
All SQL stays in the storage layer. See `docs/storage.md` for the explicit SQLite policy.

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
uv run --locked tools/corpus/build_corpus.py
```

Python is a corpus development tool. The native build, packaged application, and application startup do not need Python or `uv`.
`pyproject.toml` and `uv.lock` define the preparation environment.
Voice export uses the separate `voice-prep` dependency group. Normal resource tools use only the Python standard library.
`resources/manifest.json` records resource hashes and provenance.
`THIRD-PARTY-NOTICES.md` records licenses and edition limits.

Check the resource hashes and complete corpus glyph coverage:

```sh
uv run --locked tools/check_resources.py
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
