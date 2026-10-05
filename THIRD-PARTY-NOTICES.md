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

## Scripture excerpts

These are prototype excerpts, not final corpus or edition selections.
The project has not chosen its authoritative Greek OT, Greek NT, or English
editions. The source texts retain their numbering and wording.

- Swedish 1917: Psalms 22–25, Luke 5–7, Philippians 1–3. Historical text,
  transcribed by Scrollmapper. The transcription repository uses MIT; its license
  is in `resources/licenses/Scrollmapper-MIT.txt`.
  https://github.com/scrollmapper/bible_databases/tree/master/sources/sv/Swe1917
- English KJV: the same chapters, from Scrollmapper. Historical text is public
  domain in many jurisdictions. UK Crown printing rights can apply. This fixture
  does not establish worldwide distribution clearance for a final English package.
  https://github.com/scrollmapper/bible_databases/tree/master/sources/en/KJV
- Greek OT: Psalms 21–24 in Brenton's Greek Septuagint, from eBible.org.
  eBible identifies this text as public domain.
  https://ebible.org/grcbrent/copyright.htm
- Greek NT: Luke 5–7 and Philippians 1–3, eBible's 1904 Patriarchal Greek text
  with corrections from later editions. eBible identifies this text as public domain.
  https://ebible.org/grcbyz/copyright.htm

The Greek input pages and permission statements are archived under
`resources/corpus/provenance/`. The manifest records input URLs and SHA-256 hashes.
HTML markup and footnote links are removed during fixture extraction. No verse is
renumbered. Psalm titles that belong to verse 1 remain in that verse's text.
The fixture includes explicit MT Psalm 23 ↔ LXX Psalm 22 and adjacent Psalm mappings.

No Reformationsbibeln, Folkbibeln, or Kärnbibeln text is bundled.
Pronunciation spellings are engineering examples, not an approved pronunciation guide.

wxWidgets also links its bundled libpng, IJG libjpeg, zlib, PCRE2, and NanoSVG
libraries. Their complete notices are in `resources/licenses/`. This software is
based in part on the work of the Independent JPEG Group.
