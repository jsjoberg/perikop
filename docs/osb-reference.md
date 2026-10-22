# OSB reference authority

Perikop is a Swedish reading and read-aloud companion to the **Orthodox Study
Bible (OSB)**. The shared `osb-v1` authority defines book order, chapter/verse
coordinates and printed verse order independently of the selected translation.
Switching language, edition or pane does not switch the reference system.

The authority is metadata, not a selectable Bible translation. The reader renders
and speaks the chosen source's actual words. No OSB Bible wording, notes or articles
are distributed. The user does not need an OSB file at runtime.

## Edition and inventory

The reference is the Orthodox Study Bible, Thomas Nelson 2008. Its NT is the NKJV
and its OT the St. Athanasius Academy Septuagint. KJV is a numbering witness for
the NT, not the OT's reference authority.

`resources/corpus/osb-reference.db` contains 36,089 printed verse coordinates and
bidirectional crosswalks to all five bundled editions and locally imported SRB.
There are 78 picker entries: the books plus the named Susanna and Bel sections of
Daniel. Manasseh is part of 2 Chronicles 37. The 27 NT books contain exactly the
same 7,957 numeric coordinates as the pinned KJV witness.

Swedish book names stay familiar. Picker tooltips also identify OSB's English
names, including 1–4 Kingdoms and 1–2 Ezra.

Important differences from the old reference frames include:

| OSB | Source counterpart |
| --- | --- |
| Esther 1a–1r, then 1:1 | The lettered prelude is before numeric verse 1, not after it. |
| 1 Kingdoms 17:12 | Brenton Greek 1 Samuel 17:32; the omitted intervening passage is not renumbered the same way. |
| 2 Chronicles 37:1–15 | The sources' separate Prayer of Manasseh. |
| Nehemiah 4:1 | Greek Ezra 14:1; Swedish 1917 Nehemiah 4:7. |
| Proverbs 30:1 | Greek Proverbs 24:22f; Agur and Lemuel have their own OSB chapters. |
| Sirach 31:1 | Greek Sirach 34:1; OSB retains the Latin chapter order in 30–36. |
| Psalm 14:6 | Part of Greek Psalm 14:5. |
| John 11:36 | Swedish 1917 John 11:35. |
| 2 Corinthians 13:12–13 | One Swedish 1917 verse, 13:12; OSB 13:14 is Swedish 13:13. |
| 3 John 1:14 | Greek 1:14–15, read once as a combined unit. |

## Building the crosswalks

`resources/corpus/osb-frame.tsv` is the source of the reference: OSB's printed
coordinate order, with each coordinate's numbering witness (KJV for the NT, the
Greek Septuagint for the OT) as readable text and no Bible wording.
`build_osb_reference.py` composes those witness links with the existing reviewed
source crosswalks into links for every bundled edition. It uses only public
repository inputs, and the provenance JSON and database record their hashes.

The Maccabees retain their verse divisions, with two reviewed combined endings.
`osb-reviewed.tsv` records 106 reviewed coordinate corrections; a build writes them
back into the frame. Not every link has been checked by hand. Mismatches found by
reading alongside the printed OSB are corrected in `osb-reviewed.tsv` or the frame,
followed by a rebuild.

Adjacent OSB verses sharing an indivisible source verse form one unit. The source
text displays and speaks once, with a cue spanning those OSB verses. A repeated
passage elsewhere in OSB remains a separate occurrence. Numeric coordinate order
is never substituted for OSB's printed order when selecting, highlighting or
resuming a passage.

No same-number fallback is permitted when an OSB mapping is absent. Source-only
verses within a mapped book display faintly with their own citation, labelled
**Utan OSB-ref**; they do not enter OSB read-aloud. Books absent from OSB do not
receive invented OSB references. Unmappable legacy bookmarks remain stored and
show an explicit missing-mapping label.

## Plans and saved data

The version-3 plans follow OSB chapters and book order. They include Psalm 151,
1 Ezra and 3 Maccabees. The Prayer of Manasseh moves into the OT plan as 2 Chronicles
37. See [reading plans](reading-plans.md) for lengths and compatibility details.

Bookmarks keep their original source and coordinates; labels and opening passages
are mapped to OSB. Saved reading positions are copied to `reading:osb-v1:...` keys.
Old rows remain recovery data. Plan conversion uses frozen v1/v2 ranges and requires
all source counterparts to be read before marking a new OSB verse covered. New
material without old coverage remains unread. Checkpoints resume at the first
unread verse, so a boundary may repeat text but cannot skip an unread gap.

Each migration phase is transactional and idempotent. Existing OSB progress wins.
Reset clears all versions of a plan, and migration markers prevent resurrection.

## Reproduce and verify

Rebuild from the frame, and run the normal checks:

```sh
python tools/corpus/build_osb_reference.py
python tools/build_reading_plans.py --check
cmake --build build/cmake --parallel 2
ctest --test-dir build/cmake --output-on-failure
python tools/check_resources.py
python tests/osb_metadata_test.py
```

The installed reference database has coordinate and link tables only. Regression tests cover the reviewed exceptions, all generated
inverse links to bundled text, every lectionary assignment, mixed/merged speech,
bookmarks, v1/v2 resume conversion, rollback, reset and existing-progress precedence.
The metadata tests also rebuild the complete database from the frame and compare
all relational data, verify every NT coordinate against KJV, and check that the
shipped schema has no Bible-text fields.
