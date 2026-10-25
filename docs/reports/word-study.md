> Report as of 2026-10-14 (8d4c1c9). Not kept up to date; check the code before relying on it.

# Word study (Ordstudium)

Ordstudium turns the right pane into a lookup panel.
Select it in **Visa → Svenska och ordstudium**. It replaces the parallel language, and **Bara svenska** keeps the one-pane reader.
Click a word in the left pane to look it up. Dragging still marks verses for **Läs markering**.
Only the clicked word is highlighted. Click it again to close the highlight and lookup.

## What the panel shows

Verse source buttons appear above a clicked verse in the normal reader view.
They use the corpus's verse mapping, including differences in Psalm numbers.
Word study shows dictionary source links with their articles.

For a Swedish 1917 word:

1. The word, the IPA that the Swedish voice uses, and **Lyssna**. Read-aloud is Swedish only.
   The IPA includes bundled name phonemes and your corrections from **Granska svenskt uttal…**.
2. Matching SALDO entries show modern headwords, word classes, and semantic associations.
   Associations describe relationships between meanings. They are not dictionary definitions or guaranteed synonyms.
   Published inflections identify base forms. Reviewed historical mappings have a separate label.
   The section shows the creators, the Språkbanken source link, and the CC BY 4.0 licence link.
3. Its entries in Dalin's *Ordbok öfver svenska språket* (1850–1853).
   Dalin's Swedish is close to the 1917 text, so it also explains archaic words.
4. Matching articles from Erik Nyström's *Biblisk ordbok för hemmet och skolan* (1896).
   These cover biblical people, places, and terms. **Jesu** links to **Jesus**, and **Kristi** links to **Kristus**.
   Long articles start with a preview. Select **Visa hela artikeln** to read the full prose offline.
   The source link opens the original Runeberg article. Different people with the same name can have separate entries.

## Data

The word-study tables are part of `resources/corpus/corpus.db`.
`tools/corpus/build_corpus.py` fills them with `tools/lexicon/build_study.py` when it builds the corpus.
The inputs are committed xz-compressed in `resources/lexicon/input`. They do not ship with the application.
`tools/lexicon/fetch.py` decompresses them into `build/inputs` and checks their pinned SHA-256 hashes.
The rebuild needs no network access.

| Data | Source | License |
|---|---|---|
| Modern headwords and semantic associations | [SALDO](https://www.sprakbanken.se/resurser/saldo), `saldo.xml`, Språkbanken | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) |
| Modern inflected forms | [SALDOs morfologi](https://www.sprakbanken.se/resurser/saldom), `saldom.xml`, Språkbanken | CC BY 4.0 |
| Swedish definitions | Dalin, `dalin.xml`, Språkbanken Text | CC BY 4.0 |
| Old inflected forms | Dalin's morphology, `dalinm.xml`, Språkbanken Text | CC BY 4.0 |
| Biblical articles | Nyström, fourth edition (1896), Project Runeberg, 21 letter pages | Public domain |
| Biblical name forms | `resources/lexicon/sv1917-biblical-forms.tsv`, curated | MIT |
| Irregular 1917 forms | `resources/lexicon/sv1917-forms.tsv`, curated | MIT |

The script resolves every Swedish 1917 word form to Dalin entries at build time.
The reader only looks up the form, so the old-spelling rules do not run in the application.
The rules bridge the 1906 spelling reform: `v` for `f`/`fv`, `v` for `hv`, `kv` for `qv`, `ä` for `e` and `tt` for `dt`.
Verb endings that Dalin's morphology lacks, and a short table of strong verbs, pronouns and irregular plurals, cover the rest.
The build reports 84.8 % of the word occurrences as resolved. Most of the rest are names, and Dalin's digitization has some gaps, such as *många* and *månad*.

Nyström supplements Dalin through separate tables.
The import matches whole headwords, spelling variants, genitives, and curated aliases such as `Jesu`, `Kristi`, and `Pauli`.
It keeps entries for different people with the same name. A lookup cannot always identify the person intended in a verse.
The importer retains original prose and source URLs. It omits tables, illustrations, the modern introduction, and website navigation.
Short cross-references also show the referenced article when its target exists in the imported text.
For articles without an HTML anchor, the source link opens the corresponding letter page.
The 21 pages have pinned SHA-256 hashes in `tools/lexicon/inputs.json`.
The combined sources cover 88.2 % of Swedish 1917 word occurrences. The bundle contains 2,413 biblical articles, including cross-reference targets.

SALDO matches whole headwords and published full forms through their lemma identifiers.
Compound fragments and component forms of phrases are excluded. No suffix guesses run for SALDO.
Reviewed irregular 1917 forms supply a fallback when no published form matches.
Distinct SALDO senses remain separate. Word classes describe possible entries, rather than a grammatical analysis of the verse.

The bundle contains 12,749 SALDO senses.
SALDO matches 96.0 % of Swedish word occurrences. All three sources together match 98.2 %.
These percentages measure available entries, not prose definitions or correct interpretation of each word in context.

The build keeps only entries that the texts use.
The build is byte-for-byte reproducible.
Source dates, input hashes, citations, and licences are recorded in `tools/lexicon/inputs.json`.
Bundled third-party notices record filtering, normalization, lookup mappings, and SQLite conversion.
The full CC BY 4.0 licence text is bundled in `resources/licenses/CC-BY-4.0.txt`.

## Limits and next steps

- Dalin's definitions keep the source's OCR errors.
- Nyström is a historical source. Its spelling, chronology, place identifications, and theological interpretations reflect 1896.
  It does not serve as a statement of Orthodox doctrine. Source transcription errors can remain.
