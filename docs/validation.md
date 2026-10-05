# Validation record

Local validation date: October 5, 2026.

## Completed on macOS

The release build used Apple Clang 17 on macOS 26.6.2, on Apple Silicon.
The build targets macOS 11.0. wxWidgets 3.3.3 and SQLite 3.53.4 link statically.
The executable links only Apple system frameworks and libraries at runtime.

The core suite passed 331 checks. The GUI smoke test passed.
The GUI test draws all three themes and all four parallel modes.
It also draws a narrow three-language view. It loads the bundled fonts and uses a temporary user database.

The resource check passed all 28 file hashes.
Both Literata fonts cover all 215 distinct fixture characters, including Swedish and polytonic Greek.
The reader images received visual inspection in light and dark themes. The installed macOS bundle also passed the GUI smoke test.

The tests cover:

- Leap days, year changes, invalid date parts, and explicit date navigation.
- A stable selected date after a simulated clock change.
- Distinct Old and New calendar fixtures without a civil-date offset.
- Known reading lookup and empty results for other dates.
- Passage normalization and curated language/source selection.
- Read-only corpus access and failed write attempts.
- Required passages in all three languages and missing-text errors.
- Psalm numbering, source title differences, and a split verse range.
- Whole-token and multi-word pronunciation overrides.
- Separate display and speech text, generated introductions, and speech stub states.
- Persistent local theme, calendar, language, and font-size settings.

The renderer uses one native scrolling view.
Its layout cache retains at most 192 rows. Book coordinates contain no verse text.
The renderer retrieves text when it measures or draws a row.
A narrow viewport uses stacked language blocks.
A short viewport opens at the chapter heading to keep the prescribed text visible.
Earlier context remains accessible by scrolling upward.

## Outstanding platform checks

Windows and Linux builds did not run in this local macOS session.
The project includes `.github/workflows/build.yml` for those builds after repository publication.
The workflow also repeats the macOS build. The workflow itself has not run yet.

Windows uses pinned w64devkit 2.10.0, MinGW-w64 GCC, and Ninja.
Linux uses GCC 13, GTK3, Fontconfig, and Xvfb for the GUI smoke test.

Manual checks still include:

- Windows 11 x86-64 and Linux x86-64 startup, scrolling, and native font registration.
- Theme behavior and text shaping on each native backend.
- Multiple monitors, display scaling, sleep, and a real midnight transition.
- Startup on macOS 11 and an Intel Mac.
- A full keyboard and assistive-technology review of the custom reader.

The simulated date test checks the state model.
It does not replace a real overnight application session.
No clock timer or wake handler changes the selected date.

## Product limits

The fixture lectionary is not an authoritative Orthodox lectionary.
Fixed and Paschal cycles remain separate, unresolved fields.
The Greek excerpts are prototype editions. Final edition choices remain open.

Speech accepts utterances and shows pronunciation changes. It does not synthesize audio.
The renderer uses native Unicode shaping and greedy line breaks.
Hyphenation, paragraph-wide justification, and optical margins remain future work.
The fixture cannot substitute for a complete Scripture corpus.

Psalm 22 has source-specific title and split-verse differences.
The fixture maps those differences explicitly. Unsupported merged alignments show a quiet missing-alignment message.
The application does not infer a general MT-to-LXX numbering rule.
