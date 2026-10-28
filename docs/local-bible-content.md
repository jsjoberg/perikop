# Local Bible content

Settings offers a local import for **Reformationsbibeln 2026**. The user downloads
both PDFs from the publisher, then selects the Old Testament and New Testament files.
The app never downloads these texts or sends the chosen PDFs elsewhere.

Publisher page: https://bibel.se/wordpress/index.php/2022/01/18/oversatta-bibelbocker/

| File | SHA-256 |
| --- | --- |
| `SRBhelaGT.pdf` | `f4b3c7f1acb68dd8cb976c1a518bd5338dab8b7d8bda7340e90fdcfaaabd4b70` |
| `SRBNT-UNnoter.pdf` | `59046fcd7f3633bcfd3e46b1778712101a1c820a2714069a757619cbe8b63b75` |

These are the publisher's files retrieved on 2026-10-21. Different bytes, including
future revisions at the same URLs, are rejected. Supporting revised files requires
regenerating and checking the recipe rather than relaxing verification.

The two files install together as one Swedish source, `sv-srb2026`, with 66 books,
1,189 chapters, and 31,171 verse records. The user enables and prioritizes it in the
normal source settings. It works with chapter source selection, parallel reading,
bookmarks, plans, word study and Swedish read-aloud. Other Septuagint books retain
the existing sources because this edition does not contain them.

The imported SQLite database is `content/srb2026.db` inside the current profile.
The original PDFs need not remain after import. The bundled `corpus.db` is never
modified. Hash checking, decoding, text verification and mapping validation precede
installation; replacing content uses one transaction. A failed import preserves the
previous installation. The database is local user data and must not be packaged.

## What is extracted

The body text includes the words printed in italics. Superscript note markers,
footnotes, cross-references, page furniture and book introductions are excluded.
The 1,184 printed chapter summaries and section headings are stored separately
and displayed in centered italics before the verses they introduce. Psalm 119's
22 Hebrew letter names introduce verses 1, 9, 17, …, 169. Headings are excluded
from verse copying, word study and read-aloud. Italic styling within Scripture
itself is not retained. Psalm superscriptions that have
verse numbers are Scripture records and remain included. Footnotes can be added in
a later feature; mixing them into Scripture or read-aloud would be incorrect.

The exact PDFs use AES-128 CBC and Flate-compressed content streams. Runtime code
checks both complete-file SHA-256 hashes before decoding any streams. A pinned
extraction recipe specifies stream byte ranges, glyph encodings, text spans and
verse coordinates. Its generic font maps include ligatures and Swedish characters.
The generator uses glyph positions to recover missing spaces around note markers
and joins words broken across typeset lines. An aggregate SHA-256 verifies the
resulting normalized verse text before any database writes.

The recipe includes drop capitals in both Sabon and Times New Roman. The 0.2.0
recipe omitted the opening J in Colossians 2:1 and Jude 1:1. Importing the PDFs
again with the corrected recipe restores those initials.

The complete text audit compared all 31,171 verses with an independent Poppler
extraction of both pinned PDFs. It also repairs four missing spaces, removes
chapter/section headings from 45 verse records, and corrects punctuation,
hyphenation and spacing defects in ten further records. All discrepancies were
visually reviewed, including the encoded word-break anomaly in Revelation 16:21.
Existing imports are read exactly as stored. To apply extraction fixes, import
the same two PDFs again in Settings. There are no automatic text patches or data
migrations. See the
[complete audit](reports/srb2026-audit.md#complete-bible-text-comparison).

Older databases continue to work without headings. To add the headings, import
the two PDFs again in Settings; replacement is transactional and preserves the
previous installation if it fails. No heading text is shipped with the application:
the updated text-free recipe extracts it from the user's pinned PDFs and verifies
its own aggregate checksum before installation.

The import button stays enabled after installation, so the same PDFs can be
imported again at any time. No importer version is stored; an improved recipe
is applied only by reimporting.

No general PDF renderer, PDF JavaScript interpreter, OCR engine, Python runtime or
external PDF command is required in the installed app. Three small vendored helpers
provide AES, SHA-256 and Flate inflation. Their pins and licenses are in
`third_party/content/` and the normal license resources.

## Numbering

The product's reference authority is the **Orthodox Study Bible (OSB)**. All
sources use the same book structure and chapter/verse references, including this
import. The source-specific mappings below feed the OSB crosswalk; the selected
translation does not define the reader's numbering. See
[OSB reference alignment](osb-reference.md).

SRB uses mostly Swedish 1917 coordinates in the Old Testament and mostly KJV
coordinates in the New Testament. These choices are explicit in
`tools/content/srb_alignment.py`, with exceptions for Joshua, 1 Samuel 3–4, 2 Kings,
Ezra, Nehemiah, Psalm 49, 2 Corinthians 13, 3 John and Revelation 12:18–13:1.
The generator composes them with the existing reviewed public-domain mappings,
including Septuagint book movements and merged verses. It stores 2,522 directional
ranges, including explicit missing counterparts, in the local installation.

## Dictionary and pronunciation data

The small `swedish-study.db` supplement contains additional word-form lookups and
entries from the same pinned Dalin, Nyström and SALDO inputs as the base dictionary.
It contains no Bible verse text, footnotes or PDF-derived definitions. Runtime
queries prefer existing dictionary entries and use the supplement for missing forms.
Alternative biblical name spellings reuse existing curated pronunciations where
available, with spoken spelling aliases otherwise. User pronunciation corrections
retain their normal priority. Unlisted words use the selected voice's normal
pronunciation system. A supplemental CC0 NST lexicon supplies 4,144 previously
uncovered forms, including `klentrogna`, after the existing speech data and
corrections. Only exact lowercase dictionary forms with one supported pronunciation
are added. Ambiguous and unsupported forms retain the normal fallback. This does
not claim an individual listening review of every name or verse.
See the [SRB26 import and pronunciation audit](reports/srb2026-audit.md).

Local import avoids shipping the Bible text. It is not a legal determination about
personal copying or all possible derived data; the publisher's terms still apply.
Dictionary entries and morphology retain their own licenses and attribution.

## Reproducing and validating

PDFs and extracted diagnostics belong in an ignored local directory, never resources
or tests. The offline generator requires `pypdf==6.10.0`; runtime import does not.

```sh
python tools/content/build_srb_recipe.py build/srb --diagnostics build/srb/extracted
python tools/content/build_srb_pronunciations.py build/srb/extracted/verses.json --report build/srb/pronunciation-coverage.json
python tools/content/build_swedish_supplement.py build/srb/extracted/verses.json
cmake --build build/cmake --parallel 2
ctest --test-dir build/cmake --output-on-failure
build/cmake/bin/ortho-content-import-tests resources/corpus/corpus.db \
  resources/content/srb2026.recipe build/srb/import-test build/srb
```

The optional last argument enables real-PDF integration checks. Normal CI rejects
invalid inputs without downloading or distributing either PDF. Keep real-PDF
validation local, including rendered spot checks of drop capitals, notes, line
breaks, small caps and numbering exceptions. Inspect the aggregate text and chapter
inventory when changing the extraction rules.
