# Changelog

All notable changes to Perikop are recorded in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Version numbers follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
Builds between releases show the version and the commit, such as `0.1.0-bffaaa0`.

## [Unreleased]

The first release will be 0.1.0. Until then, this section collects everything.

### Added

- Daily readings for the North American Antiochian tradition, calculated offline, with new and old calendar modes.
- Swedish Bible 1917 with the 1921 apocrypha, the Septuagint, the 1904 Patriarchal Greek New Testament, the King James Version, and the World English Bible.
- A continuous reader framed by the Septuagint, with paragraph fitting, hyphenation, and a parallel language column.
- Go to any passage with a book, chapter, and verse picker.
- Swedish read-aloud with the Alice and Björn voices, following the text as it is read.
- Word study with Dalin's dictionary, Nyström's biblical dictionary, and Strong's Greek lexicon.
- A pronunciation review for Swedish read-aloud.
- A browser-like toolbar with back, play, pause, and stop, an address field for passages, and right-column choices.
- A translucent button that returns to the passage or to the text being read.
- Blank space after the end of a book.
- An About page with the version and links to every source the program uses.
- Reading plans on the start page: the New Testament in 30 parts; the Masoretic canon in 30 parts, with Septuagint numbering; and the books outside the Hebrew Bible that have Swedish text, in 14 parts.
- Marks for read plan parts and daily readings. Perikop asks after a part is read or listened to, and each plan can start over.
- An option in Visa to turn off the highlight of the text being read aloud.
- In the book picker, an asterisk on the books outside the Hebrew Bible and a note that names the books without text in the chosen language.
- Release packages: a DMG for macOS, an installer for Windows, and an AppImage for Linux. Each contains the voices and works offline.

### Changed

- The program is now called Perikop.
- The version is 0.1.0 plus the commit until the first release.
- The Alice and Björn voices are bundled in the application. There is no separate voice installation, and Perikop never uses the network.
- The play button shows a play symbol, and play, pause, and stop always stay in the toolbar.
