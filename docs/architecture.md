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

## Reading and speech

`reading_display` converts canon coordinates into visible book names and chapter numbers.
The reader, Bible picker, and speech introductions use the same passage labels.

`reading_speech` prepares utterances from the corpus without wxWidgets.
It accepts a pronunciation lookup function so that callers can include local corrections.
Each utterance retains its reading, section, and framing coordinates for playback tracking.
Merged verses produce one utterance that covers their full framing range.

`portable_speech` owns the synthesis worker and coordinates the audio buffer.
The engine sends numbered status updates. The window delivers these updates on the UI thread and discards stale updates.

## UI

`MainFrame` coordinates the selected day, settings, reader, and speech engine.
Its menu, playback, toolbar, and smoke-test implementations have separate files.
`ReadAloud` keeps the queue being read aloud and the text that describes its progress, without wxWidgets.
The window keeps the speech engine, the playback timer, and which reading the view follows.
`toolbar` contains the drawn toolbar controls: symbol buttons and the address field.
The Bible and date pickers return selections to the window.

`ScriptureView` owns the reader state. Its implementation has separate files for layout, selection, playback tracking, and drawing.
`controls` contains common native control helpers and UTF-8 conversions.
wxWidgets owns child windows. The C++ smart pointers own databases and the speech engine.

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
