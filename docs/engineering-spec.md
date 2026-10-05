> Implementation update, October 6, 2026: see [lectionary.md](lectionary.md) and [validation.md](validation.md). The text below preserves the original v0.1 specification.

# Orthodox Lectionary Reader — v0.1 Engineering Specification

Status: **implementation handoff / initial vertical slice**

This document describes the intended first implementation of a small native Orthodox lectionary reader for Windows, macOS, and Linux. It is deliberately opinionated and deliberately not a general-purpose Bible study platform.

## 1. Product definition

The application is a **native, offline-first Orthodox daily lectionary reader and listener**.

The primary experience is:

1. Start the application.
2. See the readings for the currently selected civil date.
3. Open a prescribed reading.
4. Read it in a beautifully typeset Bible-like view.
5. Continue above or below the prescribed passage for context.
6. Optionally display Swedish, Greek, and English in parallel.
7. Eventually listen to the prescribed readings using a curated Swedish speech system.

The application should feel like a **beloved printed Bible with quiet digital affordances**, not like a study workstation, website, IDE, or generic wxWidgets utility.

### Core design principle

> The lectionary is the doorway into Scripture; Scripture remains continuous around the prescribed reading.

A prescribed passage is highlighted or marked in the margin, but the surrounding chapter remains immediately readable.

## 2. Explicit non-goals

Do **not** turn this into Logos, e-Sword, Accordance, or a generic Bible-module platform.

For v0.1/v1, do not add:

- plugin systems;
- arbitrary user-installable Bible module formats;
- commentary libraries;
- dictionaries;
- note-taking systems;
- cloud accounts;
- synchronization;
- telemetry requirements;
- mandatory networking;
- web UI or embedded browser UI;
- Electron;
- Godot;
- Qt/QML unless wxWidgets demonstrably fails the prototype;
- mobile abstraction for Android/iOS;
- C++ modules;
- an app marketplace;
- server-backed lectionary calculation.

Strong's numbers, morphology, lexical data, and richer alignment may be explored later, which is one reason the corpus should use SQLite rather than a bespoke immutable binary format.

## 3. Longevity requirement

### Offline permanence

All core functionality must remain usable without network access for an indefinite period after installation.

A machine disconnected from the network for decades should still be able to:

- launch the program;
- determine a selected date;
- calculate the built-in church calendar;
- resolve the normal lectionary;
- read all bundled Scripture;
- display parallel texts;
- use bundled pronunciation data;
- use any already-installed/downloaded voice resources.

The application must not require login, API keys, subscription validation, online calendar APIs, a remote database, or a mandatory update service.

Optional downloadable resources are permitted, but once downloaded they must continue working locally.

## 4. Supported platforms and toolchains

Desktop only.

Primary targets:

- **Windows 11 x86-64** — w64devkit / MinGW-w64 GCC
- **Linux x86-64** — GCC, GTK3 wxWidgets backend initially
- **macOS** — Apple Clang, macOS 11+ unless testing forces a later baseline

Windows ARM64 is **not a target** for the initial project.

Use:

- C++23
- CMake
- wxWidgets
- SQLite
- ordinary `.hpp` / `.cpp` files

Do not introduce C++ modules at this stage.

The normal developer workflow should stay close to:

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake --parallel
```

A tiny `build.sh` / `build.cmd` or similar wrapper is fine, but the wrapper must remain transparent.

## 5. Skrivmotor conventions worth carrying over

Use the uploaded `skrivmotor` project as a **style reference, not a codebase to clone**.

Good patterns to retain:

- single CMake project;
- pinned dependency versions and hashes;
- `FetchContent` where appropriate;
- statically built wxWidgets;
- disable wxWidgets features that are not needed;
- ordinary native executable;
- small, transparent build scripts;
- resource manifests and third-party notices;
- source/build separation;
- tests using the same CMake tree;
- no package-manager dependency merely to compile;
- no runtime scripting-language dependency.

Do **not** inherit stale platform assumptions simply because files exist in Skrivmotor. Windows for this project is w64devkit/GCC.

Suggested wxWidgets configuration should start from the same philosophy as Skrivmotor:

- shared wxWidgets: OFF
- samples/tests/demos: OFF
- webview: OFF
- mediactrl: OFF
- OpenGL/glcanvas: OFF unless later proven necessary
- XRC: OFF unless later proven useful
- other unused wx features disabled aggressively but conservatively

Do not spend time golfing the executable size. “Small” means a normal native desktop program with no huge runtime, browser engine, or bundled neural voice model.

## 6. Proposed repository layout

```text
orthodox-reader/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── THIRD-PARTY-NOTICES.md
│
├── src/
│   ├── app/
│   │   ├── main.cpp
│   │   └── application.*
│   │
│   ├── core/
│   │   ├── date.*
│   │   ├── calendar.*
│   │   ├── lectionary.*
│   │   ├── scripture.*
│   │   ├── alignment.*
│   │   ├── reading_queue.*
│   │   └── pronunciation.*
│   │
│   ├── storage/
│   │   ├── corpus_db.*
│   │   └── user_db.*
│   │
│   ├── speech/
│   │   ├── speech_engine.*
│   │   ├── speech_text.*
│   │   └── voice_pack.*
│   │
│   ├── typesetting/
│   │   ├── paragraph_layout.*
│   │   ├── hyphenation.*
│   │   ├── text_shaping.*
│   │   └── page_metrics.*
│   │
│   └── ui/
│       ├── main_frame.*
│       ├── readings_view.*
│       ├── scripture_view.*
│       ├── margin_renderer.*
│       ├── calendar_picker.*
│       └── settings_dialog.*
│
├── resources/
│   ├── fonts/
│   ├── icons/
│   ├── corpus/
│   └── manifest.json
│
├── tools/
│   └── corpus/
│
└── tests/
```

Do not create layers merely to satisfy this diagram. If two files are enough, use two files.

## 7. Primary application state

The application has a **selected civil date**.

On program launch:

```cpp
selected_date = current_local_civil_date();
```

After that, the selected date changes **only by explicit user action**.

### Important midnight behavior

The application must **not automatically switch days at midnight**.

If the program remains open overnight, the selected date, playback, and UI remain unchanged.

A quiet optional notice may say that a new civil day is now current and offer an explicit “go to current date” action.

The same rule applies after waking from sleep.

## 8. Initial screen

Do not use a permanent tab labelled “Today / I dag”.

The primary screen is **Readings / Läsningar** for the selected date.

```text
┌──────────────────────────────────────────────────────────┐
│ ‹        Måndag 5 oktober 2026        ›   Ny kalender ▾ │
│          [feast / commemoration, if applicable]          │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  MORGON                                                  │
│  Psalm ...                                         ▶     │
│                                                          │
│  APOSTEL                                                 │
│  Efesierbrevet ...                                 ▶     │
│                                                          │
│  EVANGELIUM                                              │
│  Lukasevangeliet ...                               ▶     │
│                                                          │
│  GT / VESPERS                                            │
│  [only when prescribed]                            ▶     │
│                                                          │
│                 ▶  LYSSNA PÅ LÄSNINGARNA                │
│                                                          │
├──────────────────────────────────────────────────────────┤
│  Läsningar                              Kalender      ⚙  │
└──────────────────────────────────────────────────────────┘
```

Whether a separate top-level “Bible” area is needed is intentionally **not settled**. For v0.1, the lectionary may be the only doorway into the continuous Scripture reader.

Clicking a reading opens the continuous reader centered on that passage.

## 9. Scripture reader

The Scripture reader is the visual heart of the application.

It must not look like a text editor, HTML page, database viewer, or stack of modern UI cards.

When a lectionary passage is opened:

- center the viewport near the beginning of the prescribed passage;
- show preceding verses above it;
- show following verses below it;
- continue naturally into the rest of the chapter;
- allow continued scrolling into adjacent chapters;
- load content progressively/virtually so long books do not create thousands of native controls.

The prescribed reading is marked, but surrounding Scripture remains normal and readable.

```text
                 LUKASEVANGELIET

                         6

     26  ...

 ┃   27  Men till er som hör säger jag ...
 ┃   28  ...
 ┃   29  ...
 ┃
 ┃       DAGENS EVANGELIUM
 ┃
 ┃   35  ...
 ┃   36  Var barmhärtiga ...
 ┃
     37  Döm inte ...
     38  ...
```

The lectionary marker should live mainly in the **margin/gutter**, not by painting the entire passage with a loud background.

The view should behave as if Scripture is continuous.

Implementation may retain only a reasonable window of laid-out paragraphs/verses around the viewport and load the next/previous chapter as needed.

Do not create one wx control per verse.

Prefer a custom-drawn `ScriptureView`.

## 10. Parallel text

Bundled first-class text languages for the initial app:

- Swedish
- Greek
- English

The preferred translation/source within a language is separate from the language itself.

Two-column mode is the primary beautiful parallel layout:

```text
SVENSKA                              ΕΛΛΗΝΙΚΑ

27 Men till er som hör ...           27 Ἀλλὰ ὑμῖν λέγω ...
   ...                                  ...

28 ...                               28 ...
```

Use a quiet central gutter.

Three-column mode may exist on sufficiently wide windows but is not a primary design target.

On narrow windows, parallel text may switch to stacked/aligned blocks rather than crushing columns.

The application should not become a translation-management UI. The set of supported translations is curated by the project.

## 11. Typography and visual language

### Bundled fonts

Initial design choice:

- **Literata** — Scripture / long-form reading
- **IBM Plex Sans** — UI, metadata, dates, controls, margin annotations

Bundle the required font files with appropriate OFL license/attribution files.

Do not rely on system fonts for the main reading experience.

The exact font files and subsets should be chosen later, but ensure Swedish coverage, high-quality polytonic Greek coverage, italics where needed, and appropriate numerals and punctuation.

### Reading typography

Goals:

- generous margins;
- calm cream/paper-like light background;
- comfortable line length;
- real paragraph-wide line breaking eventually;
- high-quality hyphenation;
- hanging punctuation / optical margin behavior where feasible;
- verse numbers visually subordinate to Scripture;
- no excessive boxes, cards, shadows, gradients, or “dashboard” design.

The page should feel like a carefully typeset Bible.

### Justification

`justif` is an **inspiration/reference**, not a dependency.

Do not embed a WebView or JavaScript merely to use it.

Desired native typesetting features, in priority order:

1. correct Unicode shaping;
2. stable high-quality line measurement;
3. Swedish and Greek hyphenation;
4. paragraph-wide Knuth–Plass-style line breaking;
5. modest tracking/spacing adjustments;
6. hanging punctuation / optical margin alignment.

A native implementation may be introduced incrementally. v0.1 may start with a simpler renderer if it preserves the architecture.

Use HarfBuzz or another small dedicated shaping dependency if required for reliable polytonic Greek/custom text layout. Do not hand-roll Unicode shaping.

## 12. Theme support

Dark mode is a first-class feature.

Support exactly:

- System
- Light
- Dark

The choice is remembered locally.

Theme changes must never depend on time of day.

Dark mode should look like a **night-reading Bible**, not a terminal:

- warm near-black / charcoal background;
- soft ivory Scripture;
- muted secondary text;
- restrained gold/brass lectionary accents;
- no pure black + pure white if avoidable.

Typography and layout should remain materially the same across themes.

## 13. Calendar modes

Expose a simple user-facing switch:

- New calendar
- Old calendar

Do **not** implement “old calendar = add 13 days to everything”.

The architecture must distinguish civil date, fixed liturgical cycle, and movable/Paschal cycle.

The precise authoritative rules/jurisdiction are **not yet decided** and must not be invented by the coding agent.

Provide an interface and placeholder implementation that allows the real rules to be installed later.

```cpp
enum class CalendarStyle {
    New,
    Old
};

struct LiturgicalDay {
    CivilDate civil_date;
    CalendarStyle calendar;
    // commemorations, rank, readings...
};
```

The application must remain capable of computing the base calendar completely offline.

## 14. Lectionary

The lectionary is the application’s central domain.

However, the exact authoritative Orthodox lectionary tradition has **not yet been selected**.

Do not silently choose OCA, GOARCH, Russian, Serbian, Antiochian, etc. as authoritative.

For the initial vertical slice, use a tiny fixture/mock lectionary containing a few known dates and readings.

Design the API so later rule-based calculation can replace fixtures without touching the UI.

```cpp
struct Reading {
    ReadingKind kind;
    Passage passage;
    std::string label;
};

struct DayReadings {
    CivilDate date;
    std::vector<Reading> readings;
};

class Lectionary {
public:
    DayReadings readings_for(CivilDate, CalendarStyle) const;
};
```

Potential reading kinds:

```cpp
enum class ReadingKind {
    MorningPsalm,
    Epistle,
    Gospel,
    OldTestament,
    Vespers,
    EveningPsalm
};
```

The exact morning/evening Psalm system is also **unresolved**. Do not invent one. Use fixtures until it is decided.

Optional jurisdiction/year override files are a future feature, not required for the vertical slice.

## 15. Scripture corpus and SQLite

Use SQLite.

Split immutable shipped content from writable user state.

```text
corpus.db   read-only / replaceable
user.db     writable / user-specific
```

Open the shipped corpus read-only, preferably immutable where supported.

### Why SQLite

The initial reader needs only simple verse retrieval, but later versions may add full-text search, Strong's numbers, morphology, lemmas, word-level alignment, and richer cross-references.

Do not design a custom database format merely to save a few megabytes.

### Initial corpus schema

```sql
CREATE TABLE source (
    id              INTEGER PRIMARY KEY,
    code            TEXT NOT NULL UNIQUE,
    language        TEXT NOT NULL,
    name            TEXT NOT NULL,
    versification   TEXT NOT NULL,
    license_id      TEXT,
    attribution     TEXT
);

CREATE TABLE book (
    id              INTEGER PRIMARY KEY,
    code            TEXT NOT NULL UNIQUE,
    canonical_order INTEGER NOT NULL,
    name_sv         TEXT,
    name_el         TEXT,
    name_en         TEXT
);

CREATE TABLE verse (
    id          INTEGER PRIMARY KEY,
    source_id   INTEGER NOT NULL,
    book_id     INTEGER NOT NULL,
    chapter     INTEGER NOT NULL,
    verse       INTEGER NOT NULL,
    text        TEXT NOT NULL,

    UNIQUE(source_id, book_id, chapter, verse)
);

CREATE INDEX verse_lookup
ON verse(source_id, book_id, chapter, verse);
```

Do not prematurely compress individual verse rows.

The installer/archive will compress the database well enough. Preserve SQL simplicity.

### Alignment

MT and LXX cannot be aligned solely by `(book, chapter, verse)`.

Add a separate alignment layer.

```cpp
enum class AlignmentKind {
    Same,
    Renumbered,
    Split,
    Merged,
    Moved,
    LxxOnly,
    MtOnly
};
```

Alignment must eventually support cases such as Psalms 9/10, Psalms 114/115 vs MT 116, Psalm 147 split, Jeremiah relocation, Daniel additions, and Greek Esther.

Do not alter source Scripture text to force common coordinates.

A simple passage-level alignment table is sufficient for v0.1. Word-level alignment belongs to a later version.

## 16. Initial corpus policy

The application should distinguish between texts that may safely be bundled, desired texts requiring explicit permission, and future optional curated packages.

### Initial guaranteed bundled baseline

Target:

- Swedish 1917 / legally safe historical Swedish material;
- Greek Septuagint OT;
- suitable Greek Byzantine/Patriarchal NT;
- public-domain English fallback such as KJV / public-domain LXX English, depending on corpus side.

Exact source files, editions, provenance, hashes, and licences must be recorded in `resources/manifest.json` and `THIRD-PARTY-NOTICES.md`.

### Desired Swedish translations

The project would like to support Reformationsbibeln, Svenska Folkbibeln, and Kärnbibeln, but do **not** scrape or bundle them without a clear redistribution basis.

For Reformationsbibeln specifically, seek explicit current permission rather than relying adversarially on old permissive PDFs. The intended request is permission to include the unmodified text in a free offline Orthodox lectionary reader where the prescribed passage is marked and the user can scroll into surrounding context.

The project should behave as a respectful steward of other people’s translation work.

### Translation packages

The storage design may allow separately downloadable **curated official translation packages**, but this is not an open plugin system.

Once installed, a translation must work indefinitely offline.

## 17. Speech / synthesis subsystem

Do not make eSpeak a product dependency.

The eventual goal is an **excellent Swedish Scripture voice**, potentially closer to a deterministic speech synthesizer/voice instrument than generic black-box TTS.

The key architecture rule is:

> Display text and speech representation are separate.

Never modify the Scripture stored/displayed merely to make a speech engine pronounce it correctly.

### Speech pipeline

```text
Scripture text
    ↓
tokenization / normalization
    ↓
curated pronunciation lexicon
    ↓
phonemes / speech-friendly representation
    ↓
speech synthesizer
    ↓
audio
```

The finite Scripture corpus is an advantage. Biblical names and difficult ecclesiastical terms can be curated until there are effectively no unknown pronunciations in the bundled Swedish corpus.

Examples requiring deliberate treatment include Melkisedek, Serubbabel, Nebukadnessar, Habackuk, Maranata, and Greek/Hebrew proper names generally.

### Initial engineering interface

Do not solve the final synthesizer before the rest of the app can run.

```cpp
class SpeechEngine {
public:
    virtual ~SpeechEngine() = default;
    virtual void speak(const SpeechUtterance&) = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void stop() = 0;
};
```

```cpp
struct SpeechUtterance {
    std::string display_text;
    std::string speech_text;
    std::string language;
};
```

The v0.1 implementation may be a stub or minimal platform adapter.

Do **not** lock the design to Piper, eSpeak, or a neural runtime.

A future downloadable voice pack may contain recorded units and synthesis metadata rather than a neural model.

## 18. Pronunciation data

Pronunciation overrides are first-class corpus data.

```sql
CREATE TABLE pronunciation (
    language        TEXT NOT NULL,
    source          TEXT NOT NULL,
    spoken          TEXT,
    phonemes        TEXT,
    priority        INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY(language, source)
);
```

Exact representation may change later.

Requirements:

- match tokens/phrases, not arbitrary substrings;
- support multi-word expressions;
- support names and book titles;
- permit future phonemic representation;
- permit generated speech introductions independently of Bible text.

The speech layer should be able to say a reference naturally rather than reading abbreviations literally.

## 19. User database

`user.db` should remain small.

Potential v0.1 data:

- settings;
- last selected calendar style;
- theme;
- preferred text/source;
- reader font size;
- optional last reader location.

Do not add bookmarks/history unless needed by the prototype.

The selected date does not need to persist across launches unless later explicitly decided. On fresh launch, default to the current local civil date.

## 20. Networking

The application must function without network access.

Networking is permitted only for optional operations such as manually downloading an approved voice pack, manually downloading an approved translation package, or checking for an optional update if this is ever added.

Core startup and normal reading must never block on networking.

Do not add network code in v0.1 unless needed for an explicitly approved downloadable-resource prototype.

## 21. v0.1 vertical slice

The first implementation should prove the stack, not implement the whole church year.

### Use fixture content

Include enough fixture/real legally safe content for:

- Psalm 23;
- one additional Psalm whose LXX/MT numbering differs;
- one chapter of Luke;
- one short Epistle passage;
- Swedish;
- Greek;
- English.

A full corpus importer may come immediately after the renderer works.

### v0.1 must demonstrate

1. wxWidgets native window on all target platforms.
2. Bundled Literata and IBM Plex Sans loaded successfully.
3. Light/dark/system theme selection.
4. Date header with previous/next navigation.
5. Date does not auto-change at midnight.
6. Old/new calendar control exists, even if backed by fixture rules initially.
7. Readings screen using fixture lectionary data.
8. Open a prescribed passage.
9. Continuous reader with context before/after the passage.
10. Prescribed passage indicated primarily in the margin.
11. Swedish-only reader mode.
12. Swedish + Greek parallel reader mode.
13. Optional English parallel source.
14. Progressive/virtualized scrolling architecture.
15. SQLite corpus opened read-only.
16. One demonstrated LXX↔MT alignment case.
17. Speech abstraction compiles and can accept generated `SpeechUtterance`s.
18. One pronunciation override proves display text and speech text are separate.

Do not implement Strong's, morphology, search, plugins, or a complete lectionary for this milestone.

## 22. v0.1 visual acceptance criteria

The reader matters more than the settings dialog.

The Psalm 23 prototype should look approximately like a carefully typeset parallel Bible:

- Literata body text;
- IBM Plex Sans labels;
- warm paper background in light mode;
- warm charcoal/ivory dark mode;
- generous outside margins;
- quiet verse numbers;
- subdued gold/brass accent for lectionary marks;
- Swedish and Greek visually equal;
- no card-grid aesthetic;
- no web-browser look;
- no editor gutter look;
- no giant toolbar;
- no gratuitous gradients/shadows.

A user should plausibly want to read the page for thirty minutes.

## 23. Tests

At minimum:

### Core

- date navigation;
- selected date remains stable independent of changing “now” after initialization;
- fixture old/new calendar behavior;
- reading lookup;
- passage normalization;
- source selection;
- alignment lookup;
- pronunciation token matching.

### Storage

- read-only corpus opens;
- required fixture verses resolve;
- missing text fails cleanly;
- multiple sources can retrieve the same logical passage.

### UI smoke

- application starts;
- bundled fonts load;
- Scripture view renders;
- theme can switch;
- parallel mode can switch.

Keep testing practical. Do not introduce a giant UI-test framework unless later justified.

## 24. Code-quality rules

- Prefer plain data structures and explicit code.
- Use RAII.
- Use strong enums/types where they prevent confusion.
- Use `std::expected` where failure is expected and useful to propagate.
- Use `std::chrono` calendar types where practical.
- Keep wxWidgets types out of `core/`.
- Keep SQL out of UI code.
- Keep platform speech APIs out of core Scripture/lectionary logic.
- Avoid singleton-heavy design.
- Avoid service-locator/framework architecture.
- Avoid speculative generic abstractions.
- Avoid “provider”, “manager”, and “factory” classes unless the problem actually needs them.
- Do not add Boost unless a concrete need appears.

## 25. Open decisions — do not guess

These must remain clearly marked until decided by the project owner:

1. **Authoritative Orthodox lectionary tradition/jurisdiction.**
2. **Exact morning/evening Psalm rule or cycle.**
3. Exact Greek OT edition/data source.
4. Exact Greek NT edition/data source.
5. Exact public-domain English MT and LXX sources.
6. Current redistribution permission for Reformationsbibeln/Folkbibeln/Kärnbibeln.
7. Final Swedish speech synthesizer design and voice recording strategy.
8. Whether a separate top-level Bible browser exists in v1.
9. Exact MT/LXX alignment coverage required for v1 versus later.
10. Whether yearly jurisdiction-specific override files are a v1 feature.

The coding agent must not resolve these by preference.

## 26. First implementation order

1. Create clean CMake/wxWidgets application skeleton.
2. Bundle/load Literata + IBM Plex Sans and licences.
3. Build custom `ScriptureView` prototype with Psalm 23 fixture text.
4. Add theme system.
5. Add two-column Swedish/Greek layout.
6. Add margin-based lectionary markers.
7. Add progressive chapter/verse model.
8. Add SQLite read-only corpus layer and move fixtures into it.
9. Add date/readings screen with fixture lectionary.
10. Add one LXX↔MT alignment example.
11. Add speech/pronunciation abstraction.
12. Verify build/run on Windows w64devkit, Linux GCC, and macOS Apple Clang.
13. Only then begin full corpus import and real lectionary-rule work.

## 27. Definition of success for the first handoff

The first handoff is successful when a developer can clone the project, build it natively, launch it, and see a polished Psalm 23 reading page using the intended fonts and themes; navigate from a fixture date’s readings into that passage; scroll naturally into surrounding context; toggle Greek parallel text; and exercise a stubbed speech/pronunciation path.

At that point the foundational stack is considered proven.

Do not broaden scope before this is pleasant to use.