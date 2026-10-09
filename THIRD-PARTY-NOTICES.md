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

GNU builds include GCC's runtime libraries, under GPLv3 or later with the GCC Runtime Library Exception 3.1.
Linux packages carry the selected toolchain's `libstdc++` and `libgcc_s` as shared libraries.
The licenses are in `resources/licenses/GCC-COPYING3.txt` and `resources/licenses/GCC-RUNTIME-EXCEPTION.txt`.
Upstream source: https://gcc.gnu.org/releases.html
The Linux toolset and runtime source packages are available from Rocky Linux:
https://dl.rockylinux.org/pub/rocky/9/

## Complete Scripture editions

The package retains complete nonempty publisher texts and their source coordinates.
Empty book and verse placeholders are excluded.

- Swedish Bible 1917 with the 1921 apocrypha: 35,515 verses in 78 books.
  The canonical books come from Project Runeberg's 1917 e-text. The historical text is public domain.
  https://runeberg.org/bibeln/
  The apocrypha are curated from Runeberg's 1921 facsimile and the Scrollmapper transcription (MIT).
  https://runeberg.org/apokryf/
  https://github.com/scrollmapper/bible_databases/tree/master/sources/sv/Swe1917
- English KJV: 31,102 verses in 66 books, from Scrollmapper.
  The text is public domain in many jurisdictions. UK Crown printing rights can apply.
  https://github.com/scrollmapper/bible_databases/tree/master/sources/en/KJV
- Greek OT: 28,597 verse records in 52 books, from eBible's Brenton Septuagint.
  The publisher identifies the text as public domain.
  https://ebible.org/grcbrent/copyright.htm
- Greek NT: 7,958 verses in 27 books, from eBible's corrected 1904 Patriarchal text.
  The publisher identifies the text as public domain.
  https://ebible.org/grcbyz/copyright.htm

- World English Bible Classic: 38,029 records in 81 books, including deuterocanonical books.
  The publisher dedicates the text to the public domain. Its name remains a trademark.
  https://ebible.org/bible/details.php?id=eng-web

The source repository's `resources/manifest.json` records input URLs, revisions, and SHA-256 hashes.
The source repository keeps the complete JSON and USFM inputs for offline reproduction.
USFM notes, headings, and Strong's attributes remain outside displayed Scripture.
The importer preserves verse wording, joined verse ranges, and lettered verse labels.
Greek Daniel uses the publisher's DAG file. Greek Ezra includes Nehemiah within its source numbering.
The Swedish source has five empty book placeholders: 1 and 2 Esdras, Psalm 151, and 3 and 4 Maccabees.
These placeholders are not translations. The Greek and English editions supply available texts.
WEB publishes Greek Daniel separately and includes the Letter of Jeremiah within Baruch 6.
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

Noto Serif Hebrew uses SIL OFL 1.1.
Its unmodified font file provides explicit fallback for the Hebrew characters outside Literata.
The license is in `resources/licenses/Noto-OFL.txt`.
https://github.com/notofonts/noto-fonts

The Orthodox cross icon is original vector geometry under the application MIT license.
The vector uses Swedish flag blue and yellow. Native Cocoa produced the PNG and platform icon files.

No Reformationsbibeln, Folkbibeln, or Kärnbibeln text is bundled.
Pronunciation spellings remain engineering examples.

The Swedish pronunciation review list combines word counts from the Swedish 1917 corpus with NST reference pronunciations.
NST Pronunciation Lexicon for Swedish is supplied by Nasjonalbiblioteket Språkbanken under CC0-1.0.
The pinned input URL and hash are in `tools/lexicon/inputs.json`.
The application shows NST SAMPA as reference data and keeps personal pronunciation decisions separate.

wxWidgets also links its bundled libpng, IJG libjpeg, zlib, PCRE2, and NanoSVG libraries.
Their complete notices are in `resources/licenses/`.
This software is based in part on the work of the Independent JPEG Group.

Swedish speech uses the Alice and Björn voices and the Swedish Kokoro weights from `Joakim/kokoro-sv-voices`, under Apache-2.0.
The pronunciation model and NST-derived lexicon come from `Joakim/kokoro-sv-g2p`, under Apache-2.0.
The prepared pack includes `LICENSE-Kokoro-Swedish.txt`.
https://huggingface.co/Joakim/kokoro-sv-voices

The native Swedish front end in `src/speech/kokoro_text.cpp` reproduces the kokoro-sv text code, commit `42d1a3a5c083f405a6eb8e14c2a405ccb36cc90f`, under Apache-2.0.
https://github.com/joakimeriksson/kokoro-sv

Word study uses Dalins ordbok and Dalin's morphology from Språkbanken Text, University of Gothenburg, under CC BY 4.0.
`resources/lexicon/study.db` keeps the entries that the Swedish 1917 forms use, mapped to those forms.
https://spraakbanken.gu.se/resurser/dalin

Biblical people, places, and terms use Erik Nyström's *Biblisk ordbok för hemmet och skolan*, fourth edition (1896), digitized by Project Runeberg.
Nyström died in 1907; the original text is in the public domain.
Changes: prose extracted from the 21 letter pages, tables and illustrations omitted, unused entries omitted, Swedish 1917 forms linked to headwords.
Each bundled article retains its Runeberg source URL. Historical spelling, claims, and interpretations remain those of the source.
https://runeberg.org/biblobok/
https://runeberg.org/authors/nystreri.html
The curated lookup aliases in `resources/lexicon/sv1917-biblical-forms.tsv` use MIT.

Strong's definitions come from TBESG by STEP Bible (www.STEPBible.org), Tyndale House Cambridge, under CC BY 4.0.
The pinned source is STEPBible-Data commit `1f3423d42400f59f1f30fe08f74e38fcd3bbf7bc`.
Changes: only entries used by the Greek New Testament, definitions converted to plain text, Greek normalized to NFC.
https://github.com/STEPBible/STEPBible-Data

ONNX Runtime 1.23.2 uses MIT and includes its dependency notices.
https://github.com/microsoft/onnxruntime/releases/tag/v1.23.2

miniaudio 0.11.22 uses its public-domain option for PCM output.
https://github.com/mackron/miniaudio/tree/0.11.22

Sonic, commit `b93885dcb70aae50c6f76b0fe4e0868f029a077e`, changes playback speed without changing pitch, under Apache-2.0.
https://github.com/waywardgeek/sonic

The build copies these complete speech dependency notices into the application resources.
The application does not include macOS, Windows, or Linux system speech voices.
