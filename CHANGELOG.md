# Changelog

All notable changes to Perikop are recorded in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Version numbers follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
Builds between releases show the version and the commit, such as `0.1.0-bffaaa0`.

## [Unreleased]

## [0.2.0] - 2026-10-25

Reading-plan progress and resume positions start over; daily-reading marks, bookmarks and most settings are kept.

### Added

- OSB reference system shared by all editions.
- Reading plans in 60 NT, 182 OT and 36 other parts.
- Import of Reformationsbibeln 2026 from the publisher's PDFs.
- Bookmarks, in **Bokmärken** and the ⋮ menu.
- Ordered source preferences for both panes, with per-chapter overrides.
- Saved reading positions; Play resumes at the saved verse.
- Back and Forward, with shortcuts and mouse side buttons.
- In-app updates (**Sök efter uppdateringar…**), with release and nightly channels.
- Colour schemes: Rosé Pine (default), Gruvbox, Nord, Everforest, Catppuccin, Tokyo Night, Solarized.
- AppImage for the Raspberry Pi 5.
- AppImage runtime source (LGPL-2.1 libfuse) with Linux releases.

### Changed

- Read-aloud uses an installed eSpeak NG; the app is about 200 MB smaller.
- Swedish pronunciations stored as eSpeak phoneme codes; IPA corrections converted.
- Releases ship only self-updating packages; 0.1.x installs need one manual download.
- Linux AppImages built on Debian 13; they need glibc 2.39, as in Enterprise Linux 10.
- macOS needs Apple Silicon and macOS 15.
- Linux builds need GCC 14 instead of 15.
- AppImage runs natively on Wayland.
- New **Inställningar** page (Ctrl+,).
- ⋮ menu works like Chrome's and ends with **Avsluta**.
- One Play/Pause button and one split-view button.
- Text size applies to the whole program, 75–200 %.
- New shortcuts for selection, readings, Today, date and playback.
- Ctrl+Q quits on Windows and Linux.
- Tooltips on every button, link and menu command.
- Tab moves through toolbar, page and word study.
- Paragraph layout about 2.5 times faster.
- Typesetting avoids lone last words; poetry is set ragged right.
- HERREN and other capitals in small capitals.
- Reading column limited to about 65 characters.
- Calendar marks today and service readings, with month and year pickers.
- Leaving a reading pauses read-aloud.
- Start page uses full book names where they fit.
- Reading-plan grids share one width.
- Day and week heading stays visible.
- Address field click closes the Bible browser.
- Builds optimize for size, with LTO; releases are stripped.
- x86-64 builds target `x86-64-v2`.

### Removed

- Alice and Björn voices, ONNX Runtime and the speech audio cache.
- NSIS installer, DMG and plain AppImage.
- Ubuntu 24.04, Debian 12 and Enterprise Linux 9 support.
- Intel Mac builds.
- Akademisk ordlista from word study.
- Fasting guidance, feast markers and commemoration lists.
- Alt menu bar on Windows and Linux.
- **Idag** button beside the date.

### Fixed

- Stress on män, säg, dom, ock and words after a dash.
- Pronunciation of Jordan, spetälsk, välan, väl and about 840 names.
- Read-aloud says jo and vi as words and reads Swedish abbreviations in full.
- 25 scan-verified text errors in the 1917 Bible and 1921 apocrypha.
- Text drawn grayscale on subpixel hosts such as Ubuntu 26.04.

## [0.1.0] - 2026-10-12

The first release.

### Added

- Daily readings in Antiochian, Greek or Slavic order, new or old calendar.
- Swedish 1917 with 1921 apocrypha, Septuagint, 1904 Greek NT, LXX2012 and WEB.
- Continuous reader with paragraph fitting, hyphenation and a parallel column.
- Passage picker by book, chapter and verse.
- Swedish read-aloud with the Alice and Björn voices.
- Word study with Dalin, Nyström, SALDO and Akademisk ordlista.
- Online verse links to SFB15, Kärnbibeln, Reformationsbibeln and Bibel 2026.
- Pronunciation review for read-aloud.
- Browser-like toolbar with an address field for passages.
- Button that returns to the passage being read.
- About page with the version and source links.
- Reading plans: New Testament, Old Testament and other books.
- Read marks for plan parts and daily readings.
- Option to turn off read-aloud highlighting.
- ⋮ menu with every command.
- **Visa → Normal textstorlek**.
- Custom readings from several verse ranges in the Bible browser.
- Greek and English text for books without Swedish text.
- Greek Esther and Daniel named as in Bibel 2000.
- Chapter headings name their Swedish source.
- Release packages: DMG, Windows installer and AppImage.
- Previous and next day arrows, and **Idag**.
- Drag to mark verse ranges; **Läs markering** plays them.
- Blank space after the end of a book.

### Changed

- Program renamed Perikop.
- Windows minimum is Windows 10.
- Linux packages target glibc 2.34 and bundle the GCC runtime.
- Windows and Linux builds need GCC 15; CMake defaults to Release.
- Voices bundled; Perikop never uses the network.
- Calendar, Bible browser and About open in the main window.
- Bible browser shows books, chapters and verses on one page.
- Start page groups readings of one service.
- Fasting shown only on fast and fast-free days.
- Read-aloud announces every passage of a reading.
- Read-aloud picks the right sense of 42 ambiguous words.
- Pronunciation review filters words with several pronunciations.
- Voice model half the size; pronunciations precomputed.
- Read-aloud highlighting off by default.
- Reader width, spacing and window size follow Windows scaling.
- Full version in About and Windows file metadata.
- Icons embedded in the executable.
- Icon packaging in Python; Swift no longer needed.
- 1 John 5:7 brackets drawn as [[ ]]; Noto Sans Math dropped.
- Play, pause and stop are always in the toolbar.

[Unreleased]: https://github.com/jsjoberg/perikop/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/jsjoberg/perikop/releases/tag/v0.2.0
[0.1.0]: https://github.com/jsjoberg/perikop/releases/tag/v0.1.0
