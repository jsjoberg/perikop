# Word study (Ordstudium)

Ordstudium turns the right pane into a lookup panel.
Select it in **Visa → Höger spalt → Ordstudium**. It replaces the parallel language, and **Ingen** keeps the one-pane reader.
Click a word in the left pane to look it up. Dragging still marks verses for **Läs markering**.

## What the panel shows

For a Swedish 1917 word:

1. The word, the IPA that the Swedish voice uses, and **Lyssna**. Read-aloud is Swedish only.
   The IPA includes bundled name phonemes and your corrections from **Granska svenskt uttal…**.
2. Its entries in Dalin's *Ordbok öfver svenska språket* (1850–1853).
   Dalin's Swedish is close to the 1917 text, so it also explains archaic words.
3. Matching articles from Erik Nyström's *Biblisk ordbok för hemmet och skolan* (1896).
   These cover biblical people, places, and terms. **Jesu** links to **Jesus**, and **Kristi** links to **Kristus**.
   Long articles start with a preview. Select **Visa hela artikeln** to read the full prose offline.
   The source link opens the original Runeberg article. Different people with the same name can have separate entries.
4. In the New Testament, the tagged Greek words of the verse with their Strong's numbers and English glosses.
   An automatic word alignment preselects the Greek word that the Swedish word translates and opens its lexicon entry.
   When the alignment is unsure, nothing is preselected. Choose another word if the choice is wrong.

For a Greek New Testament word, the panel opens the Strong's entry of that exact word.
For an English word, it shows the verse's Greek words.

## Data

`tools/lexicon/build_study.py` builds `resources/lexicon/study.db` from pinned inputs.
`uv run --locked tools/lexicon/fetch.py` downloads the inputs into `build/inputs`. They are not committed.
Run `uv run --locked tools/lexicon/build_study.py` to rebuild the database.

| Data | Source | License |
|---|---|---|
| Swedish definitions | Dalin, `dalin.xml`, Språkbanken Text | CC BY 4.0 |
| Old inflected forms | Dalin's morphology, `dalinm.xml`, Språkbanken Text | CC BY 4.0 |
| Biblical articles | Nyström, fourth edition (1896), Project Runeberg, 21 letter pages | Public domain |
| Biblical name forms | `resources/lexicon/sv1917-biblical-forms.tsv`, curated | MIT |
| Strong's lexicon | TBESG, STEP Bible (Abbott-Smith definitions) | CC BY 4.0 |
| Strong's tags | The bundled Byzantine Greek New Testament (`grcbyz`) | Public domain |
| Irregular 1917 forms | `resources/lexicon/sv1917-forms.tsv`, curated | MIT |

The script resolves every Swedish 1917 word form to Dalin entries at build time.
The reader only looks up the form, so the old-spelling rules do not run in the application.
The rules bridge the 1906 spelling reform: `v` for `f`/`fv`, `v` for `hv`, `kv` for `qv`, `ä` for `e` and `tt` for `dt`.
Verb endings that Dalin's morphology lacks, and a short table of strong verbs, pronouns and irregular plurals, cover the rest.
The build reports 84.8 % of the word occurrences as resolved. Most of the rest are names, and Dalin's digitization has some gaps, such as *många* and *månad*.

Nyström supplements Dalin through separate tables. It does not change the Swedish–Greek alignment.
The import matches whole headwords, spelling variants, genitives, and curated aliases such as `Jesu`, `Kristi`, and `Pauli`.
It keeps entries for different people with the same name. A lookup cannot always identify the person intended in a verse.
The importer retains original prose and source URLs. It omits tables, illustrations, the modern introduction, and website navigation.
Short cross-references also show the referenced article when its target exists in the imported text.
For articles without an HTML anchor, the source link opens the corresponding letter page.
The 21 downloaded pages have pinned SHA-256 hashes in `tools/lexicon/inputs.json`.
The combined sources cover 88.2 % of Swedish 1917 word occurrences. The bundle contains 2,413 biblical articles, including cross-reference targets.

The script also trains a Swedish–Greek word aligner on the New Testament.
It is a fast_align-style IBM Model 2 on lemmas (Dalin entries and Strong's numbers), trained in both directions with add-n smoothing.
A link is kept when both directions give it a probability of at least 0.5.
On a hand-checked sample of 63 words, 53 links were right, 1 was wrong and 9 words were left unlinked.
Verses pair through the corpus alignment, so the 1917 New Testament verse divisions are respected. See [versification](versification.md).

The build keeps only the Dalin entries and Strong's entries that the texts use.
It converts the TBESG definitions to plain text and normalizes Greek to Unicode NFC, as the Scripture text.
The build is byte-for-byte reproducible.

## Limits and next steps

- Strong's data covers only the New Testament. The Septuagint text has no word tags.
  STEPBible's tagged Hebrew text (TAHOT) could serve the Old Testament, but the reader's Old Testament follows the Septuagint and its numbering.
- The word alignment is statistical. It links about half of the New Testament words, mostly content words, and leaves uncertain words unlinked.
- The source omits Strong's tags for a few words and for some quotations. The panel says so.
- Dalin's definitions keep the source's OCR errors. The Strong's definitions are in English, because no free Swedish Greek lexicon exists.
- Nyström is a historical source. Its spelling, chronology, place identifications, and theological interpretations reflect 1896.
  It does not serve as a statement of Orthodox doctrine. Source transcription errors can remain.
