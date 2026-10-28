# SRB26 import and pronunciation audit

2026-10-27. Inputs are the two publisher PDFs pinned in
[local Bible content](../local-bible-content.md), and the CC0 Swedish NST dictionary
pinned in `tools/lexicon/inputs.json`. PDFs and extracted verse text remain local
diagnostics under the ignored `build/srb` directory.

## Missing chapter initials

The screenshot of Colossians 2:1 showed its opening J missing. The printed PDF has
a Times New Roman drop capital, while the extractor accepted Sabon body text.
Inspecting all 1,630 PDF pages found exactly two large Times New Roman letters:
Colossians 2:1 (NT PDF page 330) and Jude 1:1 (NT PDF page 401).

After accepting those drop capitals, comparing all 31,171 extracted verses with
the previous recipe changes only those two records, each by inserting its initial
J. All 66 books, 1,189 chapters, verse inventories and numbering remain unchanged.
The generator now rejects a lowercase chapter opening, which catches these losses.
This is a check of chapter initials, not a complete transcription review.

The runtime also corrects reads of the exact old import checksum
`06e9957d4b860bfdfc9e70b24df6f23c2332f19314e45c9d0cd88ae5035fc625`.
It does not modify local databases; new imports use the corrected recipe directly.
The real-PDF integration test checks both paths and confirms the legacy file stays
unchanged.

## Complete Bible text comparison

2026-10-28. The initial Gospel pass has now been extended to **all 66 books,
1,189 chapters and 31,171 verses**. Both complete Scripture sections were checked:
OT PDF pages 3–1202 and NT PDF pages 4–428. The comparison uses the actual C++
importer's SQLite output, not only the Python generator's diagnostic text.
The PDFs have the exact SHA-256 hashes in [local Bible content](../local-bible-content.md).
The independent engine is Poppler `pdftohtml` 26.06.0, rather than the importer's
pypdf recipe. No missing verse, Scripture letter or Scripture word remains among
the reviewed differences.

The requested [srbibeln.net reader](https://srbibeln.net/biblereader.php) showed
visitor verification. The publisher also
[links to Bibelonline](https://bibel.se/wordpress/index.php/bibelonline/), whose
start-page title still says **Reformationsbibeln 2016**. That label alone does
not identify the edition of the current text. Follow-up checks found that the
online reader matches the 2026 PDF at two places where the publisher's actual
[2016 NT PDF](https://www.bibel.se/objfiles/1/SRBNTmedUNnoter_-774953843.pdf)
differs: Matthew 1:6 uses `genom Urias hustru` rather than `med Urias hustru`,
and Matthew 4:7 uses `fresta Herren din Gud` rather than `sätta Herren, din Gud,
på prov`. This supports a stale page title, not a demonstrated 2016-only text.
Matthew 1 and Luke 13 were inspected online during the first pass; further spot
checks covered Psalm 119:1–9, 1 Chronicles 1:1 and Revelation 16:21. A complete
online comparison was not performed: the publisher's pinned 2026 PDFs remain the
edition authority for this audit.

| Testament | Books | Chapters | Verses | Exact matches after repair | Reviewed layout differences |
| --- | ---: | ---: | ---: | ---: | ---: |
| Old | 39 | 929 | 23,213 | 23,153 | 60 |
| New | 27 | 260 | 7,958 | 7,928 | 30 |
| Total | 66 | 1,189 | 31,171 | 31,081 | 90 |

### Confirmed extraction defects

The Gospel pass repaired four missing spaces across the shared extractor:
`er,ska` in Luke 13:3 and 13:5, `dig.Du` in Exodus 15:7, and `lagar.Och` in Exodus
24:3. This is why Exodus appeared in the Gospel report; those two passages were
checked individually before this complete Old Testament comparison.

The complete comparison corrects **54 additional verse records**:

| Defect | Verses | References |
| --- | ---: | --- |
| Chapter/section furniture mixed into Scripture | 44 | 1 Samuel 22:1; 2 Samuel 12:1, 13:1, 14:1; 1 Chronicles 1:1, 2:1, 4:1, 13:1, 22:1, 25:1; Psalm 28:1; Psalm 119:1 and 8, 16, …, 168; Proverbs 2–10, 30 and 31 (verse 1) |
| Standalone dashes mistaken for word hyphenation | 5 | Joshua 18:28, 19:7, 21:29, 21:32; Isaiah 7:20 |
| Cross-reference punctuation appended to Scripture | 2 | Genesis 37:28; Numbers 33:9 |
| Spaces splitting a word | 2 | Jeremiah 29:17; Revelation 16:21 |
| Missing thousands separator | 1 | Revelation 11:3 |

All affected printed passages were rendered and inspected. The 44 furniture
records include incomplete chapter-summary fragments and the 22 Hebrew alphabet
section labels in Psalm 119. The latter had mostly been appended to the *preceding*
verse. They are unnumbered headings; numbered Psalm superscriptions remain
Scripture and are retained.

The root causes and repairs are:

- Chapter summaries and alphabet labels sometimes use a smaller Sabon font rather
  than the expected Times font. Exclude that smaller text while retaining adjacent
  Scripture punctuation. Retain genuine body text just under 10 pt, including
  2 Chronicles 2:14, and the small period in Romans 6:5.
- Preserve a standalone dash across a line break while still joining hyphenated
  words. The five restored dashes are visibly separated from the preceding word.
- Reject orphan body-font punctuation on cross-reference lines, including the
  two stray periods that had produced double sentence endings.
- Raise geometric gap recovery from 1.0 to 1.1 pt. The former threshold falsely
  inserted a space in Jeremiah's `så`; the rebuilt complete text confirms that
  no other word boundary changes through this threshold adjustment.
- Preserve explicit large PDF text advances between digits. A scan of all PDF
  text arrays found exactly one such case, the printed `1 260` in Revelation 11:3.
- Revelation 16:21 encodes a stray space before the discretionary hyphen inside
  `människorna`. The rendered word is unambiguous. A documented, coordinate-specific
  exception joins this word; it is not a missing parenthetical dash. All body
  line endings with a space before a hyphen were inspected for this distinction.

Relative to the original import, the total repair covers **60 verse records**:
these 54, the four spaces from the Gospel pass, and the two previously restored
initial Js. No book/chapter/verse coordinates change, and no Scripture words are
added or removed. The removed words are chapter/section furniture.

### Remaining differences and scope

Every remaining discrepancy was reviewed against rendered text:

- 86 are Poppler adding spaces inside visibly letter-spaced words.
- Two are Poppler omitting a word space retained correctly by the import:
  Nehemiah 12:30 and Isaiah 3:11.
- One is spacing before a colon in Nehemiah 10:1. The import retains the PDF's
  awkward printed `var :Ståthållaren`; this audit does not rewrite publisher typography.
- One is the PDF's encoded hyphenation anomaly in Revelation 16:21 described above.
  The import correctly reads `människorna`; the independent extractor reports the
  encoded `män - niskorna`, so this remains a visible, reviewed content discrepancy.

Thus 89 residual differences contain identical non-whitespace characters, and
one has the manually resolved discretionary hyphen. The checker reports all 90;
it does not silently whitelist them. Full text, character differences, page
coordinates and rendered review crops remain local under ignored `build/srb`.

The checker does not filter Scripture by font family. It accepts the observed
body/italic/drop-cap sizes and separates smaller summaries, notes, running headers
and cross-references by size, position and reference syntax. It checks the complete
chapter/verse inventory independently of the imported text. Line wrapping,
hyphenation, punctuation spacing and exceptional font sizes were reviewed. This
verifies extraction from the pinned PDF, not editorial accuracy of the publisher's
translation or a collation against another complete edition.

### Compatibility and validation

The final aggregate text SHA-256 is
`b49864c521ff402b4a467f6f1dbd87523b1d12f29c2891d16f80e4f9d4f4c72c`.
Read-time recovery supports exactly these three older import checksums:

- `06e9957d4b860bfdfc9e70b24df6f23c2332f19314e45c9d0cd88ae5035fc625`
- `8c5d5d59ee520d2afd8444e82ed55f4bbb787c65a6404ece6f36e640262da65c`
- `74c69d2a991658418cdb0ba9050cd76e19624a36ca55321a7b3f58e03180ceec`

The generated compatibility table contains only coordinates, byte offsets,
deletion lengths and generic space/dash/J insertions. It contains no Bible or
summary text. The repair generator verifies each older diagnostic text becomes
the current text exactly. Existing user databases are not rewritten, and the PDFs
are not needed again for these repairs. Unknown checksums receive no edits.

Real-PDF integration checks import all 31,171 verses, pin representative corrected
verses and smaller-font Scripture, and exercise all three older repair paths.
Synthetic old byte ranges use placeholder bytes for removed furniture. For each
path, all 31,171 runtime verses must equal the current import and the database file
must remain unchanged. An unknown-checksum case must retain its original data.
The independent checker also has offline regression tests for unfamiliar fonts,
italic words, notes, punctuation gaps, orphan periods, hyphenation, smaller body
text, Psalm superscriptions, skipped verse numbers and UTF-8 byte repairs.

### Reproduce the complete comparison

```sh
python tools/content/build_srb_recipe.py build/srb --diagnostics build/srb/extracted
python tests/srb_audit_test.py
build/cmake/bin/ortho-content-import-tests resources/corpus/corpus.db \
  resources/content/srb2026.recipe build/srb/import-test build/srb
python tools/content/audit_srb.py build/srb \
  build/srb/import-test/content/srb2026.db build/srb/full-audit
```

The checker exits 1 for differences requiring review, including the 90 documented
layout differences. Its `comparison.json` contains per-book counts and every
remaining discrepancy. PDFs and full-text diagnostics must remain untracked.
`tools/content/build_srb_repairs.py` regenerates the compatibility header from
local previous/current `verses.json` snapshots, whose aggregate hashes it records;
those text snapshots are not distribution resources.

## Pronunciation coverage (2026-10-27)

The previous speech pack covered the older Swedish Bible vocabulary. SRB26's
modern and differently inflected forms can therefore reach eSpeak's own spelling
rules even when a related spelling has prepared phonemes. `klentrogna` occurs four
times and was absent, while `klentrogen` and `klentrogne` already had entries.

| Check | Before | After |
| --- | ---: | ---: |
| Distinct word forms in the corrected import | 21,249 | 21,249 |
| Forms without an exact prepared pronunciation or correction | 5,410 | 1,266 |
| Occurrences of those forms | 50,587 | 4,166 |

These compare exact spellings; name-genitive handling and phrase corrections may
cover additional occurrences without their own dictionary rows.

The additional 4,144 entries come from exact lowercase NST forms with a single
supported eSpeak pronunciation. Multiple or unsupported variants are not guessed.
The new file is byte-sorted and loaded with the existing compact lexicon reader,
after the original lexicon, name-genitive handling and precomputed corpus entries.
Existing pronunciation corrections continue to apply first.

`klentrogna` receives `kl'e:ntR,u:gna`, matching the compound stress of the existing
related forms. Tests check case handling, ambiguous and unsupported entries, and
the runtime lookup. These are dictionary and synthesis checks, not a listening
review of every added form. The remaining 1,266 forms still need review where
eSpeak's fallback sounds wrong; missing prepared data does not mean a word is
omitted from audio.

## Reproduce

```sh
python tools/content/build_srb_recipe.py build/srb --diagnostics build/srb/extracted
python tools/content/build_srb_pronunciations.py build/srb/extracted/verses.json --report build/srb/pronunciation-coverage.json
python tests/srb_pronunciation_test.py
build/cmake/bin/ortho-content-import-tests resources/corpus/corpus.db resources/content/srb2026.recipe build/srb/import-test build/srb
```

The coverage report lists forms without exact entries for local follow-up. Only
independent dictionary word forms and phonemes are distributed, with no SRB26 verse
text or PDFs.
