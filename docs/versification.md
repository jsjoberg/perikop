# Swedish text and Septuagint alignment

The Orthodox Church reads the Old Testament in the Septuagint.
The Swedish 1917 Bible translates the Hebrew text and uses its own verse numbers.
The reader therefore aligns each edition explicitly. It never forces common coordinates.

## Swedish sources

The 66 canonical books come from Project Runeberg's 1917 e-text, `runeberg-bibeln-1917.html`.
`tools/corpus/runeberg1917.py` keeps the printed 1917 verse numbers.
It removes chapter summaries, cross references, and translator footnotes.

The 1917 verse numbering comes from an older tradition than its chapter headings.
Some verses therefore print under the heading of the next chapter, such as Job 39:34–38.
The verse number, not the heading, decides the chapter.
Joshua 21:36–37 and some New Testament verses appear only in footnotes. The corpus omits them.

The 1921 apocrypha are in `sv1921-apokryfer.tsv`.
`tools/corpus/apocrypha1921.py` derives this file from the Scrollmapper transcription and Runeberg's proofread 1921 facsimile OCR.
The OCR is the textual witness. The transcription supplies the verse divisions, because the OCR margin numbers are unreliable.
Every correction in the script names the verse and was checked against the OCR page.
`apocrypha1921.py --check` lists remaining differences of three or more words.
The 1921 edition omits Sirach's prologue and some verses of the longer Greek Sirach. The corpus follows it.

## Corrections to the earlier Swedish input

The earlier input, `Swe1917.json`, forced the 1917 text into KJV chapter lengths:

- Verses beyond the KJV count were merged into the last verse, as in Psalms 22:31–32 and 51:19–21.
- Shorter chapters left empty slots, so verses such as Wisdom 1:16 and Sirach 22:27 were missing.
- Bel 1–22 were missing. The additions to Esther 6:15–7:11 were interleaved and partly missing.
- About 510 verses contained translator footnotes and note markers, such as `[1] Hebr. adám`.
- Chapter summaries, cross references, and leaked verse numbers appeared inside verses.
- Periods after names such as Dan, Job, and Rut were removed, and some lines were truncated, as in Micah 3:6.

## Alignment

`tools/corpus/align_editions.py` writes `resources/corpus/alignment.tsv`. Run `build_corpus.py` before and after it.
A Gale–Church alignment compares verse lengths and shared proper names.
`tools/corpus/align_rules.py` contains the reviewed versification knowledge:

- Units order a book as the Septuagint does, for example Jeremiah, Proverbs, 3 Kingdoms, and Sirach 30–36.
- Rules state where an edition follows another chapter division. The alignment strongly prefers these numbers.
- Overrides state transpositions and links that a monotonic alignment cannot find.

The 1917 Bible follows the Hebrew Psalm titles, but in places the KJV or Latin chapter divisions.
Brenton's Greek usually follows the Hebrew divisions. Each difference is a rule.

Jeremiah shows the largest differences:

| Swedish (Hebrew order) | Septuagint |
| --- | --- |
| 25:1–13 | 25:1–13 |
| 49:34–39 (Elam) | 25:14–20 |
| 46 (Egypt) | 26 |
| 50–51 (Babylon) | 27–28 |
| 47 (Philistines) | 29 |
| 49:7–22, 1–6, 28–33, 23–27 | 30 |
| 48 (Moab) | 31 |
| 25:15–38 | 32:15–38 |
| 26–44 | 33–51:30 |
| 45 | 51:31–35 |

The Greek also lacks about one eighth of the Hebrew text, for example 33:14–26 and 39:4–13.
Verses without a counterpart show "Saknas i Septuaginta".

The table aligns Swedish with the Septuagint, the KJV, and WEB.
The KJV and WEB reach the Septuagint through the Swedish alignment. WEB numbers the Old Testament like the KJV.
New Testament books keep common coordinates.

## Presentation in Septuagint numbering

The reader presents the Old Testament in the Septuagint's order and numbering, as the Orthodox Study Bible does.
`src/core/canon.cpp` lists the books in OSB order. The Greek edition frames each book: its chapters and verses make the rows, in every pane.
Each pane shows its own edition's text for those verses through the alignment.
Where an edition numbers a verse differently, the pane shows the LXX number with the edition's own number in small print, such as `13 (33:13)`.
Verses that only the Hebrew text has follow the LXX verse they come after. They are muted, marked `hebr.`, and not read aloud.
Brenton joins Ezra and Nehemiah as 2 Esdras 1–23. The reader shows 2 Esdras 11–23 as Nehemiah 1–13, and Greek Esther as Esther.
Books that the left pane's edition lacks, such as 1 Esdras in Swedish, are greyed out in the book picker.
The New Testament keeps common coordinates.

## Lectionary references

Orthocal references use KJV numbering. References that the KJV cannot address use Septuagint numbering.
These include the deuterocanonical books, Daniel 3:24–90, and readings marked LXX.
The importer records the reference edition for each reading rule.
The reader maps each reading into Septuagint numbering.
For example, Jeremiah 31:31–34 is Septuagint 38:31–34, and Baruch 3:35 is Swedish 3:36.
The Holy Saturday reading Daniel 3:1–88 shows Swedish Daniel 3:1–23 and Asarjas bön in the Swedish pane.
A mapped passage includes verses that only the target edition has, such as lettered Septuagint additions.
