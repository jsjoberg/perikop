# Changelog

All notable changes to Perikop are recorded in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Version numbers follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
Builds between releases show the version and the commit, such as `0.1.0-bffaaa0`.

## [Unreleased]

### Added

- An AppImage for the Raspberry Pi 5 with 64-bit Raspberry Pi OS. Other Raspberry Pi models and other ARM Linux systems are not supported.

### Changed

- The ⋮ menu works like Chrome's: commands in one list, with **−** and **+** rows for text size and read-aloud speed. Arrow keys, Tab, Return, and Escape work in it.
- Text size is one percentage, 75–200 %, for the whole program instead of the reading text only. Earlier text size settings reset to 100 %.
- The theme, calendar, reading order, voice, and highlighting are on a new **Inställningar** page (Ctrl+,). The menu bar is now **Visa**, **Gå**, **Uppläsning**, and **Hjälp**.
- Tab moves through the toolbar, the page, and the word study column in that order, and no longer stops at the first toolbar button. The date on the day page can be reached with Tab.
- The reader lays out paragraphs about 2.5 times faster, so new text appears sooner while scrolling.
- The Linux AppImage runs natively on Wayland desktops instead of through XWayland. Set `GDK_BACKEND=x11` to use X11.
- Navigation state, playback commands, and reader control rules have separate components. Toolbar and menu actions use the same policy.
- Clicking the address field in the Bible browser closes it and restores the previous view.
- Leaving a reading pauses read-aloud. Opening another reading ends the previous playback session. Paused playback controls are disabled outside the reader.
- The month calendar marks today and dates with Orthros, Vespers, and other service readings. Tooltips list services and reading counts.
- The month and year in the calendar open grids to choose a month or year. The current month and year are marked like today.
- Typesetting avoids a single word alone on a paragraph's last line. Poetry and narrow columns are set ragged right, and poetry indents turned-over lines.
- HERREN and other words in capitals appear in small capitals and are not hyphenated.
- The single reading column is limited to about 65 characters on all displays.
- Fasting guidance, fixed feast headings and markers, and commemoration lists are removed. Internal day ranks still select the prescribed readings.
- The reading-cycle day and week heading stays visible, centered with the date controls and month calendar.
- The separate Idag button beside the date is removed. The menu action and keyboard shortcut remain available.
- Native builds and CI default to size optimization, with link-time optimization when supported.
- x86-64 builds target `x86-64-v2` for X220-era CPUs. Speech kernels select newer CPU instructions at runtime.
- Release packages strip executable symbols and the bundled macOS ONNX Runtime. Build binaries keep their symbols for debugging.
- Hyphenation patterns compile into C++ data. Installed resources no longer contain TeX files.

### Removed

- Akademisk ordlista from word study. SALDO already shows the same headwords and word classes for almost every word it matched, and some of its matches were wrong.

### Fixed

- Swedish read-aloud stresses Jordan as Jordán, no longer reads ai as ä in Betsaida and about 40 other names, and stresses spetälsk and spetälska on the second syllable.
- Opening the book picker no longer crashes on Wayland when no pointer device is present.


## [0.1.0] - 2026-10-12

The first release.

### Added

- Daily readings in the Antiochian, Greek, or Slavic reading order, calculated offline, with new and old calendar modes.
- Swedish Bible 1917 with the 1921 apocrypha, the Septuagint, the 1904 Patriarchal Greek New Testament, and in English the LXX2012 Septuagint with the World English Bible New Testament, as on Orthocal.
- A continuous reader framed by the Septuagint, with paragraph fitting, hyphenation, and a parallel language column.
- Go to any passage with a book, chapter, and verse picker.
- Swedish read-aloud with the Alice and Björn voices, following the text as it is read.
- Word study with Dalin's dictionary and Nyström's biblical dictionary.
- Word study shows modern headwords, word classes, and semantic associations from SALDO, and academic vocabulary from Akademisk ordlista.
- Online verse buttons in word study for Folkbibeln SFB15, Kärnbibeln, Reformationsbibeln, and Bibel 2026.
- A pronunciation review for Swedish read-aloud.
- A browser-like toolbar with back, play, pause, and stop, an address field for passages, and right-column choices.
- A translucent button that returns to the passage or to the text being read.
- Blank space after the end of a book.
- An About page with the version and links to every source the program uses.
- Reading plans on the start page: the New Testament in 30 parts; the Masoretic canon in 30 parts, with Septuagint numbering; and the books outside the Hebrew Bible that have Swedish text, in 14 parts.
- Marks for read plan parts and daily readings. Perikop asks after a part is read or listened to, and each plan can start over.
- An option in Visa to turn off the highlight of the text being read aloud.
- A ⋮ menu at the right of the toolbar with every menu command. Windows and Linux hide the menu bar; Alt shows it.
- **Visa → Normal textstorlek** returns the text to its default size.
- The left column is always Swedish. **Visa** offers Bara svenska, or Swedish with word study, Greek, or English.
- In the Bible browser, **Lägg till** stacks selected ranges as the parts of one reading, and **Rensa** clears them.
- Books without Swedish text open in the Bible browser for their Greek and English text.
- The Greek Esther and Daniel are named "Ester enligt den grekiska texten" and "Daniel enligt den grekiska texten", as in Bibel 2000.
- Chapter headings name the Swedish 1917 Bible, the 1921 apocrypha, or both, after the chapter's Swedish text.
- Release packages: a DMG for macOS, an installer for Windows, and an AppImage for Linux. Each contains the voices and works offline.
- Arrows beside the date for the previous and next day, and an **Idag** button for the current date.
- Custom readings in the Bible browser: with **Flera intervall**, add verse ranges from different books in playback order, then open or play them.
- Drag across verses in the reader to mark a range. Ctrl or Command adds more ranges. **Läs markering** plays them in Scripture order.

### Changed

- CMake defaults to Release and selects an installed GCC 15 pair on Linux. Local build commands need fewer options.
- A portable Python tool packages the existing icon artwork. Swift is no longer required for icon preparation.
- The single-column reader width accounts for font resolution on Windows. Page spacing and the initial window size also follow display scaling.
- Read-aloud text highlighting is off by default. Saved choices still apply.
- Windows executable and installer metadata show the full release version, including prerelease suffixes.
- Application icons are embedded in the executable. Installed resources no longer contain an icons folder.
- The Windows minimum is now Windows 10 x86-64, including read-aloud.
- Windows and Linux builds require GCC 15 or later. CI uses stable GCC 15 through the Linux distribution toolset and w64devkit 2.7.0.
- Linux packages target glibc 2.34 through an enterprise Linux 9 build container, bundle the GCC runtime, and reject newer glibc dependencies.
- The program is now called Perikop.
- The About page shows the release version, such as 0.1.0-alpha.2, in a release build. Other builds show the version and the commit.
- The Alice and Björn voices are bundled in the application. There is no separate voice installation, and Perikop never uses the network.
- The play button shows a play symbol, and play, pause, and stop always stay in the toolbar.
- The Greek text of 1 John 5:7 marks its bracketed passage with [[ ]] instead of ⟦ ⟧, in the reader's own font. Perikop no longer includes the Noto Sans Math font.
- The calendar, Bible browser, and About page open in the main window instead of separate dialogs. Back and Escape return to the previous view, and playback continues.
- The Bible browser shows book, chapter, and verse grids on one page. A verse opens immediately.
- In the Bible browser, a book's chapters unfold directly under its row, and a chapter's verses unfold under the chapter. Click again to fold them. With **Flera intervall**, the pending range is shaded.
- The start page no longer names the reading order, and it shows fasting only on fast days and fast-free days.
- On the start page, readings of the same service and occasion form one reading, such as the three Vespers prophecies.
- Read-aloud announces every passage of a reading before it begins, not only the first.
- Read-aloud pronounces 38 frequent words with more than one pronunciation in the sense they have in the Bible, such as "han", "du", "hon", "kan", "son", "dom", "ben", and "kors". The voice used the pronunciation of a name, an abbreviation, or another word.
- Read-aloud pronounces "förlåten", "hov", "bete", and "dans" in the sense of each verse: the temple veil or "forgiven", a royal court or the past tense of "häva", pasture or "bete sig", and a dance or the tribe of Dan. Bundled phrase entries such as "innanför förlåten" and "Dans stam" choose the sense.
- The pronunciation review has a filter for the 303 Bible words with several pronunciations in NST. Each pronunciation can be heard in its verse with one click.
- The voice model is half the size and uses less memory. Pronunciations for the Bible text are prepared in advance.

[Unreleased]: https://github.com/jsjoberg/perikop/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/jsjoberg/perikop/releases/tag/v0.1.0
