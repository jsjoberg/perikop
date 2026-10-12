# Missing Swedish Bible passages

Audit date: 2026-10-11. Source commit: `29aaa0c4a79a37055e1bf69580e0f7733d49c642`.

The bundled Swedish source contains the 1917 Bible and the 1921 apocrypha: 35,515 records in 78 book codes.
This inventory compares every bundled Greek and English verse with the Swedish text through the current alignment.
It preserves each source edition's numbering. A letter identifies a separate portion of a Greek verse.

The inventory covers whole works and verse records without a Swedish counterpart.
It does not compare every word within verses that already have a counterpart.
Some aligned verses contain additional Greek or English phrases absent from the Swedish edition.

## Entire works without Swedish text

| Work | Extent | Bundled records | Place in the reader |
| --- | --- | ---: | --- |
| Första Esdrasboken (1 Esdras) | Chapters 1–9 | 448 Greek; 448 WEB | Main canon |
| Tredje Mackabeerboken | Chapters 1–7 | 228 Greek; 228 WEB | Main canon |
| Psalm 151 | Verses 1–7 | 7 Greek; 7 WEB | Main canon |
| Fjärde Mackabeerboken | Chapters 1–18 | 483 Greek; 484 WEB | Additional bundled work |
| Andra Esdrasboken (2 Esdras) | Chapters 1–16 | 944 WEB | Additional bundled work |

Here, 2 Esdras means the separate apocalyptic work in WEB.
Greek Ezra–Nehemiah, also called 2 Esdras in some editions, already has Swedish counterparts.

## Smaller omissions in the Swedish Bible

The following passages lack a complete verse in the bundled Swedish source.
The New Testament follows the 1917 edition's omissions.
The source importer also omits translator footnotes.

| Book | Missing passages | Numbering |
| --- | --- | --- |
| Josua | 21:36–37 | Swedish / KJV |
| Nehemja | 7:68 | Swedish / KJV |
| Matteusevangeliet | 17:21; 18:11; 21:44; 23:14 | Swedish / KJV; Greek and WEB use 23:13 for the last passage |
| Markusevangeliet | 7:16; 9:44; 9:46; 11:26; 15:28 | KJV / WEB; the bundled Greek edition also omits 7:16 |
| Lukasevangeliet | 17:36; 23:17 | Greek / KJV; Swedish 17:36 corresponds to Greek 17:37 |
| Johannesevangeliet | 5:4 | Greek / KJV / WEB |
| Apostlagärningarna | 8:37; 15:34; 24:7; 28:29 | Greek / KJV; WEB also omits some of these verses |
| Romarbrevet | 16:24 | Greek / KJV / WEB |

Swedish 1 John 5:7 exists, but lacks the heavenly witnesses clause found in KJV and bracketed in the bundled Greek text.
This is a partial-verse difference, not an absent Swedish verse.

## Septuagint passages without a Swedish counterpart

These are the complete remaining Greek Old Testament ranges that the current alignment marks without a Swedish counterpart.
Some are Greek additions. Some require a mapping correction or further text review.
The whole Psalm 151 appears above.
Sirach 1:1–1g is the Greek prologue. Swedish Sirach 1:1 maps to Greek 1:1h.

| Book | Greek references without a Swedish counterpart |
| --- | --- |
| Andra Moseboken | 35:12a; 38:18–21; 38:27; 39:12 |
| Josua | 15:59a; 21:36–37; 21:42a–42d; 24:31a; 24:33a–33b |
| Första Samuelsboken | 30:28a |
| Andra Samuelsboken | 5:16a; 23:37–37a |
| Första Kungaboken | 2:35a–35b; 2:35d–35o; 2:46b–46l; 5:14a–14b; 6:36a; 9:9a; 10:22a–22c; 10:26a; 12:24a–24z; 16:28a–28h |
| Andra Kungaboken | 1:18a–18d |
| Andra Krönikeboken | 35:19a–19d; 36:2a–2c; 36:4a; 36:5a–5d |
| Job | 2:9a–9e; 19:4a; 23:15a; 36:28a–28b; 42:17a–17e |
| Psaltaren | 144:13a |
| Ordspråksboken | 3:16a; 3:22a; 4:27a–27b; 6:8a–8c; 6:11a; 7:1a; 8:21a; 9:10a; 9:12a–12c; 9:18a–18d; 10:4a; 12:11a; 12:13a; 13:9a; 13:13a; 15:18a; 15:27a; 15:28a; 15:30; 17:6a; 17:16a; 18:22a; 22:8a; 22:9a; 22:14a; 24:22a–22e; 25:10a; 25:20a; 26:11a; 27:20a; 27:21a; 28:17a |
| Tobit | 11:19 |
| Jesus Syraks vishet | 1:1–1g; 7:17a–17b; 12:4; 12:7; 30:13a–13b |

## Known mapping problems: Swedish wording already exists

These entries also appear in the raw audit. They are not whole verses that require a new Swedish translation.

| Source reference | Existing Swedish wording or review finding |
| --- | --- |
| `grc-lxx` Exod 38:21 | Swedish Exod 38:20 contains the bronze pegs. The current mapping omits this counterpart. |
| `grc-lxx` Exod 38:27 | Compare Swedish Exod 40:30–32: the basin and washing by Moses, Aaron, and his sons. |
| `grc-lxx` 2Sam 23:37 | Swedish 2 Sam 23:37 names the Ammonite Selek. Greek names Elie. The lettered additions need separate review. |
| `grc-lxx` Prov 15:30 | The Swedish wording is present in Prov 15:30. |
| `grc-lxx` Tob 11:19 | The seven-day wedding celebration is present in Swedish Tob 11:18. |
| `grc-patriarchal` Acts 19:41 | The dismissal of the assembly is present at the end of Swedish Acts 19:40. |
| `en-web` Acts 19:41 | The dismissal of the assembly is present at the end of Swedish Acts 19:40. |
| `en-kjv` 1Cor 16:24 | Swedish 1 Cor 16:24 contains the verse. KJV also appends an editorial subscription. |
| `en-kjv` 2Cor 13:14 | The verse is Swedish 2 Cor 13:13. KJV also appends an editorial subscription. |
| `en-kjv` 2Tim 4:22 | Swedish 2 Tim 4:22 contains the verse. KJV also appends an editorial subscription. |
| `en-web` Tob 7:18 | The consolation of Sarah is present in Swedish Tob 7:17. |
| `en-web` Sir 3:27 | The stubborn heart and accumulated sins are present in Swedish Sir 3:26. |
| `en-web` Baruch 6:72 | The rotting garments and destruction of the idols are present in Swedish Baruch 6:71. |
| `en-web` DanGr 3:76 | The plants blessing the Lord are present in Swedish PrAzar 1:51. |

## Additional WEB ranges without a Swedish counterpart

These ranges add candidates beyond the whole works and New Testament omissions above.
Known mapping problems appear in the previous table.

| Book | WEB reference | Current assessment |
| --- | --- | --- |
| Tobit | 7:18 | Swedish text exists at 7:17; mapping problem |
| Jesus Syraks vishet | 3:27 | Swedish text exists at 3:26; mapping problem |
| Jesus Syraks vishet | 12:7 | No Swedish counterpart in the current alignment |
| Baruk / Jeremias brev | 6:72 | Swedish text exists at Baruk 6:71; mapping problem |
| Daniel, Greek additions | 3:76 | Swedish text exists at Asarjas bön 1:51; mapping problem |

## Texts that are already present under other book names

- Jeremias brev: Swedish Baruk chapter 6.
- The prayer and song in Daniel 3: Swedish Asarjas bön.
- Susanna and Bel and the Dragon: separate Swedish books.
- Greek Esther additions: Swedish Tillägg till Ester.
- Nehemiah in Greek Ezra chapters 11–23: the Swedish book Nehemja.

## Complete verse inventory

[missing-swedish-passages.tsv](missing-swedish-passages.tsv) contains all 3,513 affected source records, including duplicates across editions.
Each row contains the edition, reference, Greek or English source text, audit reason, review status, and note.
The `whole_work_absent` status identifies works without any bundled Swedish text.
The `no_swedish_counterpart` status identifies gaps in the current alignment, which can include additional mapping problems.
The `existing_text` statuses identify wording already present in Swedish.

Corpus SHA-256: `4c20f4f5fa1ebb01313100dfa5fc355a877d6ab709f2da566a4b9c1acd187fd1`.
