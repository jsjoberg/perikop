# Changelog

All notable changes to Perikop are recorded in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Version numbers follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
Builds between releases show the version and the commit, such as `0.1.0-bffaaa0`.

## [Unreleased]

### Added

- **Enkel röst**, a third read-aloud voice in **Inställningar**: a small formant synthesizer with Swedish vowels, retroflexes and a tapped r, using the same Swedish pronunciations as Alice and Björn. It sounds robotic but stays intelligible, needs no voice model, and is fast enough for any computer. Alice and Björn are unchanged.
- Passage-selection actions for bookmarks and Swedish read-aloud alongside the online Bible links. Bookmarks use a ribbon icon, persist in the local profile, and appear in Bible book order in **Bokmärken**, including a submenu beside the main ⋮ menu. Opening a bookmark selects its saved passage; removing one requires confirmation.

- Ordered, enabled/disabled source preferences for the main text and the second pane. Swedish is first by default, English is the fallback, and Greek main-text editions are opt-in. The second pane defaults to English, then Greek, with Swedish opt-in. Each chapter has source dropdowns for temporary independent pane overrides. Non-Swedish main text has no read-aloud.

- Reading positions save automatically on pause, navigation, and close. Play resumes at the beginning of the saved verse, Stop resets the reading, and daily readings and reading-plan sections show the same passage progress percentages. Completed readings are marked without a confirmation dialog.
- Back and Forward buttons, platform keyboard shortcuts, and mouse side-button navigation retain the current session's opened views and reading positions. A new destination clears Forward; navigation history is not persisted.
- Linux releases and nightlies carry the corresponding source of the AppImage runtime, which statically links libfuse under the LGPL-2.1, so it can be rebuilt and relinked. The AppImages include the complete LGPL-2.1 text and the licences of every library in the runtime.
- **Sök efter uppdateringar…** in the ⋮ menu, just before pronunciation review, opens an update dialog and checks the chosen channel. Downloading and restarting to install each wait for you to choose them. The update-channel preference remains in **Inställningar**. Velopack installers and AppImages support this; the original packages remain available for manual installation. Releases and nightlies publish separate platform feeds, full packages, and deltas, with checksums. macOS 15 update compatibility must pass baseline validation before release.
- Colour schemes in **Inställningar**: Rosé Pine (the default), Gruvbox, Nord, Everforest, Catppuccin, Tokyo Night, and Solarized. They replace the earlier Perikop colours, and a saved Perikop choice becomes Rosé Pine. Each has a light and a dark variant that follows the theme setting, and a preview shows both. Colours have roles: surfaces for fields and hovered controls, a selection colour, verse numbers in the scheme's red, and calendar colours.
- A saved update-channel choice in **Inställningar**: releases by default, or experimental nightly builds. Changing it makes no network requests.
- An AppImage for the Raspberry Pi 5 with 64-bit Raspberry Pi OS. Other Raspberry Pi models and other ARM Linux systems are not supported.

### Changed

- Windows and Linux use the ⋮ menu and keyboard shortcuts; the Alt menu-bar toggle has been removed.
- One toolbar button switches between Play and Pause. One split-view button replaces the separate Greek and English buttons and follows the saved secondary-source priority. Word study uses a distinct text-and-magnifier icon.

- All reading-plan button grids fill the same content width, with or without completion marks and percentages.
- macOS releases require Apple Silicon and macOS 15 or later. Intel and universal builds are retired.
- CI uses uv-managed Python 3.14 and one Python build entry point. Windows builds always compile on Linux and run native checks on Windows; the migration fallback has been removed. Voice export retains its separate Python 3.12 environment.
- Passage selection uses Ctrl/⌘+L, daily readings Ctrl/⌘+Shift+L, Today Ctrl/⌘+Shift+T, date selection Ctrl/⌘+Shift+D, and playback Ctrl/⌘+Shift+P. Previous/next day use Ctrl/⌘+PageUp/PageDown, leaving the macOS bracket shortcuts for Back/Forward.
- Nightly builds have versions that sort in order, such as `0.1.1-nightly.20261018.412.1`, for in-app updates. **Om Perikop** also shows the commit.
- Björn is the default read-aloud voice for new profiles. Profiles that already saved settings keep their voice.
- The ⋮ menu ends with **Avsluta**, as in Chrome. On Windows and Linux, Ctrl+Q also quits.
- Readings and the reading plan on the start page show whole book names, such as "Första Korintierbrevet 11:23–32", when they fit on one line. Longer references keep the short names.
- Every button, link, and menu command has a tooltip, including **Alla läsplaner**, the Bible browser's buttons, and the word study panel.
- The Project Runeberg 1917 e-text and the saved eBible pages are kept in zip files, like the other source texts. `resources/manifest.json` keeps the hash of each original file.
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

- On Wayland, the ⋮ menu stays under its button when the text size changes. It used to keep its old left edge and grow past the window.
- Swedish read-aloud stresses Jordan as Jordán, no longer reads ai as ä in Betsaida and about 40 other names, stresses spetälsk and spetälska on the second syllable, and stresses välan on -an. Välsignen no longer sounds like "väl" (well) + signen. The modal "väl" (surely), as in "Kan väl en blind leda en blind?", has a short ä; "väl" meaning "well" keeps its long ä. About 100 other names follow the stress rules of the 1921 Church Bible, such as Rehábeam, Gámliel, Daríus, and Dekápolis. About 700 more names follow its general rules: Ébal and Ábdon, not Ebál and Abdón; Baésa and Jáeser no longer read ae as ä; Us and Kus are no longer spelled out.
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
