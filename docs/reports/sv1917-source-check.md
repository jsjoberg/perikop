# Swedish 1917 source check

Checked 2026-10-25. This is a source comparison, not a complete proofreading of the Bible.

## Confirmed text corrections

The [Swedish errata draft](sv1917-errata.md) now records 15 scan-verified passages, including the two initial corrections below. It provides source links, page locations and unchecked boxes for human verification before sharing upstream. One further wording difference is held for edition comparison.

| Verse | Imported text | Printed text | Witness |
| --- | --- | --- | --- |
| Jeremiah 8:15 | V bida efter frid | Vi bida efter frid | [1927 printing, p. 1017](https://runeberg.org/bibeln/efs1927/gt/1017.html) |
| Sirach 28:9 | leva l frid | leva i frid | [1921 printing, p. 106](https://runeberg.org/apokryf/0110.html) |

Both were checked visually against the scanned pages. The import scripts apply literal, verse-specific corrections and fail if the expected old text is absent. The original downloaded inputs retain their pinned hashes.

The standalone v and l pronunciation workarounds have been removed. The four remaining v occurrences are in `o. s. v.` in Ezra 4:10, 4:11, 4:17 and 7:12. Read-aloud expands that abbreviation to “och så vidare”; it does not speak its letters or interpret them as numerals.

Common Swedish abbreviation expansions live with the other word corrections in `resources/corpus/pronunciation-sv.tsv`. A `spoken` third column supplies words instead of phonemes. Dots are required, capitalization is ignored, and spaces between abbreviation parts are optional. The displayed Scripture retains its abbreviation.

The `jo` correction selects the interjection's pronunciation. NST already distinguishes uppercase `JO`, pronounced as letters (`""ji:$%u:`), from lowercase `jo`, tagged `IN` and pronounced `"ju:`. The bundled voice lexicon selected the uppercase entry under lowercase `jo`. `JO` likely means [Justitieombudsmannen](https://www.jo.se/om-jo/); NST's entry does not spell out the abbreviation's meaning. This is a choice between existing NST entries, rather than a missing pronunciation in NST.

## MySword comparison

The [official MySword catalog](https://www.mysword.info/download-mysword/bibles) links the [Swedish 1917 module](https://mysword-bible.info/download/getfile.php?file=swe1917.bbl.mybible.zip). The downloaded module identifies itself as version 1.0.1, dated 2014-12-03. Its own metadata says it derives from Project Runeberg release 3.7, dated 1999-04-09. It is another packaging of the same textual lineage, not an independent witness. It has no apocrypha.

- ZIP SHA-256: `75d4d32c23478ec67d3ca7532cfc332bcaf65025d9cb780ece2507205991b408`
- SQLite module SHA-256: `4659984c2b547ca4ca17ff743a17addfe2da533734b700a523464c41cc504eec`

The comparison removes module formatting, chapter titles and footnotes, normalizes whitespace, and matches exact book/chapter/verse coordinates. It keeps punctuation and case for the first comparison, then compares word sequences ignoring punctuation and case. It does not silently remap verse numbers or merge records.

| Result | Verses |
| --- | ---: |
| Canonical verses in Perikop | 31,150 |
| Nonempty Scripture records in MySword | 31,132 |
| Shared coordinates | 31,132 |
| Same text after formatting normalization | 30,313 |
| Same word sequence, differences in case/punctuation/spacing | 700 |
| Different word sequences | 119 |
| Coordinates present only in Perikop | 18 |

The 18 coordinate differences include verses embedded in a preceding MySword record, such as 3 John 1:15 and Daniel 3:31–33. They are not automatically classified as missing Scripture.

The [word differences](sv1917-mysword-differences.tsv) are review candidates. MySword still has “V bida” in Jeremiah 8:15. It also has conversion damage: “spärra” becomes “ia” in Lamentations 2:16, and some records include chapter summaries or embedded verse labels. These differences do not justify replacing Perikop's corpus with MySword. Neither corpus is certified correct by this comparison; shared transcription errors cannot be detected by comparing two copies of the same source.

The scan review added 12 canonical corrections that were shared with MySword, increasing the comparison's word-difference count from 107 to 119. These 12 differences are documented corrections, not new unverified findings. The apocrypha correction is outside MySword's coverage.

[Runeberg documents](https://runeberg.org/bibeln/) remaining proofreading errors and mixed printed exemplars in its e-text. Its scanned editions provide a direct witness for reviewing corrections. [Wikisource](https://sv.wikisource.org/wiki/Bibeln_1917) and [Språkbanken](https://sprakbanken.se/resurser/bibel1917) also identify Runeberg as their source; they are not independent validation of every word.

To repeat the comparison after downloading and extracting the MySword module:

```sh
uv run --locked tools/corpus/compare_mysword.py /path/to/swe1917.bbl.mybible
```

It writes a summary with input hashes and coordinate differences to `build/corpus/audit/mysword-summary.json`, and the differing verse wordings to `build/corpus/audit/mysword-differences.tsv`. It never changes the corpus. The remaining candidates need review against scans before further wording changes.
