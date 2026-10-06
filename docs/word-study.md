# Word study (Ordstudium)

Ordstudium turns the right pane into a lookup panel.
Select it in **Visa → Höger spalt → Ordstudium**. It replaces the parallel language, and **Ingen** keeps the one-pane reader.
Click a word in the left pane to look it up. Dragging still marks verses for **Läs markering**.

## What the panel shows

For a Swedish 1917 word:

1. The word, the IPA that the Swedish voice uses, and **Lyssna**.
   The IPA includes bundled name phonemes and your corrections from **Granska svenskt uttal…**.
2. Its entries in Dalin's *Ordbok öfver svenska språket* (1850–1853).
   Dalin's Swedish is close to the 1917 text, so it also explains archaic words.
3. In the New Testament, the tagged Greek words of the verse with their Strong's numbers and English glosses.
   Choose the Greek word that matches the Swedish word to open its lexicon entry.

For a Greek New Testament word, the panel opens the Strong's entry of that exact word.
For an English word, it shows the verse's Greek words.

## Data

`tools/lexicon/build_study.py` builds `resources/lexicon/study.db` from pinned inputs.
`uv run --locked tools/lexicon/fetch.py` downloads the inputs into `build/inputs`. They are not committed.

| Data | Source | License |
|---|---|---|
| Swedish definitions | Dalin, `dalin.xml`, Språkbanken Text | CC BY 4.0 |
| Old inflected forms | Dalin's morphology, `dalinm.xml`, Språkbanken Text | CC BY 4.0 |
| Strong's lexicon | TBESG, STEP Bible (Abbott-Smith definitions) | CC BY 4.0 |
| Strong's tags | The bundled Byzantine Greek New Testament (`grcbyz`) | Public domain |
| Irregular 1917 forms | `resources/lexicon/sv1917-forms.tsv`, curated | MIT |

The script resolves every Swedish 1917 word form to Dalin entries at build time.
The reader only looks up the form, so the old-spelling rules do not run in the application.
The rules bridge the 1906 spelling reform: `v` for `f`/`fv`, `v` for `hv`, `kv` for `qv`, `ä` for `e` and `tt` for `dt`.
Verb endings that Dalin's morphology lacks, and a short table of strong verbs, pronouns and irregular plurals, cover the rest.
The build reports 84.8 % of the word occurrences as resolved. Most of the rest are names, and Dalin's digitization has some gaps, such as *många* and *månad*.

The build keeps only the Dalin entries and Strong's entries that the texts use.
It converts the TBESG definitions to plain text and normalizes Greek to Unicode NFC, as the Scripture text.
The build is byte-for-byte reproducible.

## Limits and next steps

- Strong's data covers only the New Testament. The Septuagint text has no word tags.
  STEPBible's tagged Hebrew text (TAHOT) could serve the Old Testament, but the reader's Old Testament follows the Septuagint and its numbering.
- The Swedish text has no word links to the Greek. The panel lists the verse's Greek words, and you choose the matching one.
  A statistical Swedish–Greek word alignment could point at the probable word.
- The source omits Strong's tags for a few words and for some quotations. The panel says so.
- Dalin's definitions keep the source's OCR errors. The Strong's definitions are in English, because no free Swedish Greek lexicon exists.
