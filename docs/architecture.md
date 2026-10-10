# Native code structure

The application uses C++23 and wxWidgets. Python tools prepare resources and check the packaged data.

| Directory | Responsibility |
| --- | --- |
| `src/app` | Application startup, resources, fonts, and command-line arguments. |
| `src/core` | Dates, calendar rules, canon coordinates, and passage labels. |
| `src/storage` | SQLite access, database policies, and queries. |
| `src/speech` | Pronunciation, speech preparation, synthesis, and audio playback. |
| `src/typesetting` | Paragraph fitting, hyphenation, and text measurements. |
| `src/ui` | Native controls, dialogs, menus, and reader interaction. |
| `tests` | Native regression tests and resource installation tests. |

## Storage

`database.hpp` exposes the database interfaces. Each database has a separate implementation file.
`sqlite.hpp` and `sqlite.cpp` contain internal connection, statement, and transaction helpers.
Statements and transactions own their cleanup and cannot be copied.
All SQL stays in this layer. The [storage guide](storage.md) describes the database policies.
Passage mapping reuses target-book coordinates when it joins adjacent verses.

## Reading and speech

`reading_display` converts canon coordinates into visible book names and chapter numbers.
The reader, Bible picker, and speech introductions use the same passage labels.

`reading_speech` prepares utterances from the corpus without wxWidgets.
It accepts a pronunciation lookup function so that callers can include local corrections.
Each utterance retains its reading, section, and framing coordinates for playback tracking.
Merged verses produce one utterance that covers their full framing range.
Each speech batch compiles pronunciation rules once per language and reuses them for every verse.

`portable_speech` owns the synthesis worker and coordinates the audio buffer.
The engine sends numbered status updates. The window delivers these updates on the UI thread and discards stale updates.
Audio device initialization runs outside the lock used by speech controls and playback polling.
Pause suspends further preparation after the current chunk finishes. Resume wakes the worker and preserves queued audio.
Fast synthesis maintains about 30 seconds of queued audio instead of continuously preparing ten minutes ahead.
Slower synthesis retains a larger lead, up to ten minutes, based on measured synthesis and playback speed.
The synthesis worker releases played audio chunks. The audio callback does not free buffers or retain the entire ring's playback history.
ONNX Runtime sessions park worker threads between inference calls and retain normal thread coordination during inference.
Word pronunciation uses a separate worker. A new word replaces pending requests, and the UI discards results for an earlier word.
Panel resizing and theme changes reuse the current word's prepared pronunciation.
Book names stay in immutable corpus metadata, so playback status polling does not query SQLite for each update.

## UI

`MainFrame` coordinates the selected day, settings, reader, and speech engine.
Its menu, playback, toolbar, and smoke-test implementations have separate files.
`ReadAloud` keeps the queue being read aloud and the text that describes its progress, without wxWidgets.
The window keeps the speech engine, the playback timer, and which reading the view follows.
`toolbar` contains the drawn toolbar controls: symbol buttons and the address field.
The Bible picker returns selections to the window.
`month_calendar` is the start page's drawn month calendar. It reports the chosen day to the window.

`ScriptureView` owns the reader state. Its implementation has separate files for layout, selection, playback tracking, and drawing.
`controls` contains common native control helpers and UTF-8 conversions.
wxWidgets owns child windows. The C++ smart pointers own databases and the speech engine.

## Reader layout

Scripture appears in a custom native view with continuous chapter context.
The reader scrolls in pixels and preserves macOS trackpad precision and momentum.
Native paragraph fitting uses Knuth–Plass demerits, Liang hyphenation, and fractional native glyph measurements.
Prose flows across verses. Small inline numbers preserve verse navigation.
Punctuation protrudes into optical margins.
The renderer retains book coordinates and at most 192 text layouts.
It creates no native control for individual verses.
The [corpus guide](corpus.md#display-metadata-and-limits) describes paragraph metadata and display limits.

The reader caches transparent text tiles with eight lines per tile.
The tile cache uses native display pixels and retains at most 32 MiB of pixel data.
Scrolling copies cached tiles instead of drawing every word again.
Speech highlights and selections appear beneath the text tiles.
The Windows renderer uses grayscale antialiasing for transparent text.

Font, width, and DPI changes invalidate paragraph layouts and text tiles.
Color changes invalidate text tiles without discarding paragraph layouts.
The layout cache retains the most recently used paragraphs.
A timer prepares nearby paragraphs, one paragraph per event, between input events.
A direct jump can still require synchronous layout of the visible paragraphs.

Paragraph measurement reuses native fonts and measured word widths, including hyphenated fragments.
Width caches distinguish font attributes, DPI, display scale, and graphics renderer.
Four font configurations retain separate body and marker widths.
Hyphenation caches are separate for each language and reset when patterns reload.
Each cache retains at most 2,048 entries and 256 KiB of estimated key and value payload, plus container overhead.
Drawing also reuses native fonts when inline verse numbers or muted text change the font.

Playback caches the geometry of the spoken verse.
The animation uses elapsed time and stops after the marker and opacity settle.
Repeated paused or buffering updates do not request reader frames.
The renderer still uses native graphics contexts. It does not use a GPU glyph atlas.

The build patches a wxWidgets 3.3.3 macOS bug in its locale month and weekday names.
The bug over-released Swedish month and weekday names and caused a crash.

## Development checks

If `clang-format` is on the executable search path during CMake setup, CMake provides these targets.
The repository uses `.clang-format` for all native source files, tests, and C++ tools.
The current format pass uses clang-format 22.1.8.

Format the native code:

```sh
cmake --build build/cmake --target format
```

Check the native code format:

```sh
cmake --build build/cmake --target format-check
```

If `clang-tidy` and `run-clang-tidy` are on the search path during CMake setup, CMake also provides a `tidy` target.
It runs the checks in `.clang-tidy`, including the Clang Static Analyzer, on the native sources, tests, and C++ tools.
Build the application first. Any finding is an error. CI uses the pinned macOS tools from its dependency snapshot.

```sh
cmake --build build/cmake --target tidy
```

To keep a finding that is wrong, add `// NOLINT(check-name)` with the reason, on the line or with `NOLINTNEXTLINE` before it.

Build the application:

```sh
cmake --build build/cmake --parallel
```

Run the regression tests:

```sh
ctest --test-dir build/cmake --output-on-failure
```

The `reading-speech` test checks framing numbers, merged verses, pronunciation corrections, sections, explicit editions, and read-aloud status text without audio playback.
The `ui-smoke` test checks native rendering and playback presentation.
