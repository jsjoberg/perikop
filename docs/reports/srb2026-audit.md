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

## Pronunciation coverage

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
