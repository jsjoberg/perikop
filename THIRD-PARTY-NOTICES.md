# Third-party notices

The application code uses the MIT license. These resources have their own terms.

## Fonts

Literata, by TypeTogether for Google, uses SIL OFL 1.1. The bundled files are
unmodified static Regular and Italic fonts. The license is in
`resources/licenses/Literata-OFL.txt`.
Source: https://github.com/googlefonts/literata

IBM Plex Sans, by IBM and Bold Monday, uses SIL OFL 1.1. The bundled files are
unmodified static Regular and Medium fonts. The license is in
`resources/licenses/IBMPlexSans-OFL.txt`.
Source: https://github.com/IBM/plex

## Libraries

wxWidgets 3.3.3 uses the wxWindows Library Licence. This license permits static
linking of applications under their own terms. The complete license and third-party
notices are copied from the fetched source into packaged resources at build time.
Source: https://github.com/wxWidgets/wxWidgets/releases/tag/v3.3.3

SQLite 3.53.4 is dedicated to the public domain. It is built from the pinned
amalgamation with loadable extensions disabled.
Source: https://sqlite.org/copyright.html

## Complete Scripture editions

The package retains complete nonempty publisher texts and their source coordinates.
Empty book and verse placeholders are excluded.

- Swedish Bible 1917 with historical apocrypha: 35,350 verses in 78 books, from Scrollmapper.
  The historical text is public domain. The transcription repository uses MIT.
  https://github.com/scrollmapper/bible_databases/tree/master/sources/sv/Swe1917
- English KJV: 31,102 verses in 66 books, from Scrollmapper.
  The text is public domain in many jurisdictions. UK Crown printing rights can apply.
  https://github.com/scrollmapper/bible_databases/tree/master/sources/en/KJV
- Greek OT: 27,860 verses in 51 books, from eBible's Brenton Septuagint.
  The publisher identifies the text as public domain.
  https://ebible.org/grcbrent/copyright.htm
- Greek NT: 7,958 verses in 27 books, from eBible's corrected 1904 Patriarchal text.
  The publisher identifies the text as public domain.
  https://ebible.org/grcbyz/copyright.htm

The manifest records input URLs, revisions, and SHA-256 hashes.
Complete JSON and USFM inputs remain bundled for offline reproduction.
USFM notes, headings, and Strong's attributes remain outside displayed Scripture.
The importer preserves verse wording and source numbering.
No modern Antiochian website Scripture translation is copied into the corpus.

## Calendar rules

Recurring Greek and shared pericope tables and the native rule port derive from Orthocal.
The selected revision is `5bdf0a5e1cad406388d7860ec74ed506a7a19197`.
Copyright (c) 2022 Brian Glass. MIT license: `resources/licenses/Orthocal-MIT.txt`.
https://github.com/brianglass/orthocal-python

Antiochian Scripture citations are factual references from the official 2026 chart and daily reading metadata.
The package excludes Slavic-specific tables and liturgical composite wording.

## Hyphenation and fallback fonts

The complete, unmodified CTAN pattern files retain their copyright and license headers.
The native Liang and paragraph-fitting code uses the application license.
The Justif project supplied a design reference. No JavaScript or web runtime is embedded.
https://github.com/lyallcooper/justif

- Swedish patterns: Copyright (C) 1994 Jan Michael Rynning; LPPL 1.2 or later.
  The package includes LPPL 1.3c in `resources/licenses/Hyphenation-LPPL-1.3c.txt`.
- Ancient Greek patterns: Copyright (C) 2008–2016 Dimitrios Filippou; MIT option selected.
- American English patterns: Copyright (C) 1990, 2004, 2005 Gerard D.C. Kuiken.
  Their file header permits redistribution with the copyright and permission notices.

Noto Serif Hebrew and Noto Sans Math use SIL OFL 1.1.
Their unmodified font files provide explicit fallback for corpus characters outside Literata.
The license is in `resources/licenses/Noto-OFL.txt`.
https://github.com/notofonts/noto-fonts

The Orthodox cross icon is original vector geometry under the application MIT license.
The vector uses Swedish flag blue and yellow. Native Cocoa produced the PNG and platform icon files.

No Reformationsbibeln, Folkbibeln, or Kärnbibeln text is bundled.
Pronunciation spellings remain engineering examples.

wxWidgets also links its bundled libpng, IJG libjpeg, zlib, PCRE2, and NanoSVG libraries.
Their complete notices are in `resources/licenses/`.
This software is based in part on the work of the Independent JPEG Group.
