# OSB reference alignment

Perikop is a Swedish reading and read-aloud companion to the **Orthodox Study
Bible (OSB)**. OSB defines the displayed book structure, order and chapter/verse
references. A translation is a text source aligned to those references; changing
the preferred translation must not change the reference system.

## Evidence from the supplied edition

The local reference examined on 2026-10-22 was the Thomas Nelson 2008 OSB EPUB,
ISBN 9781418578633, supplied by the user. Its introduction identifies the New
Testament as NKJV and the Old Testament as the St. Athanasius Academy Septuagint,
based on Rahlfs with Brenton and NKJV among its sources. Neither KJV nor Brenton
alone is the reference authority for the complete OSB.

The verse-anchor inventory of all 27 NT books matches the pinned KJV witness:
7,957 coordinates, with no missing or extra coordinates. This makes the KJV a
useful NT numbering witness. An inventory match alone does not prove that every
verse has the same content boundaries; semantic crosswalks still need checks.

Reproduce this inventory comparison with `tools/corpus/audit_osb.py`, passing the
local EPUB path and `--output build/osb/reference-audit.json`. The report contains
only numeric anchor coordinates and the input hash, never OSB wording. It is
deliberately incomplete for OT lettered additions and cannot serve as the runtime
master inventory or prove semantic alignment.

Concrete differences from the application's current frame include:

| OSB reference | Current frame or source | Consequence |
| --- | --- | --- |
| John 1 ends at 51 | Patriarchal Greek ends at 52 | Map verse divisions, do not copy coordinates. |
| 2 Corinthians 13 ends at 14 | Swedish 1917 and Patriarchal Greek end at 13 | OSB 13:12–13 share their counterpart's 13:12; OSB 13:14 corresponds to 13:13. |
| 3 John ends at 14 | Swedish 1917 and Patriarchal Greek end at 15 | The final verse has a different division. |
| 2 Chronicles 37:1–15, Prayer of Manasseh | Separate `PrMan` book | Show the OSB location and map the existing source book. |
| Nehemiah 1–13 | Brenton's combined Ezra 11–23 | Use a separate OSB book and explicit chapter mappings. |
| Nehemiah 3 ends at 37; chapter 4 ends at 17 | Current Hebrew/Greek chapter divisions differ | Subtracting ten from Ezra's chapter number is insufficient. |
| Psalm 14 ends at 6; Psalm 115 ends at 9 | Brenton's Greek ends at 5 and 10 respectively | Matching Septuagint psalm numbers does not guarantee matching verse divisions. |
| Esther has internal lettered additions, including 1a–1r | Existing Greek Esther frame | Preserve lettered coordinates and check the sequence, not just numeric anchors. |
| 2 Chronicles 35 and 36 end at 31 | Brenton's Greek ends at 27 and 23 | Audit content and divisions; do not truncate to the current inventory. |

These examples are an audit, not a completed crosswalk. The supplied EPUB also
has internal articles, study notes and variant markers. Those must not be mistaken
for verse text or verse coordinates when extracting reference metadata.

## Current implementation and merge gate

`CorpusDb::frame_source` currently chooses Brenton's Greek for the OT and a
language-specific source for the NT. `canon.tsv` follows OSB order in broad terms,
but adapts that order to Brenton's structure. This is not exact OSB alignment.
The reading-plan split PR changed part boundaries and progress conversion; it did
not implement or establish an OSB reference frame.

The SRB import is draft work. Its extraction and source mappings
can be tested independently, but it is not ready to merge as the finished OSB
companion feature. Completion requires:

1. A versioned, text-free OSB coordinate inventory and reviewed crosswalks to every
   source. Keep the reference authority separate from selectable text sources.
2. The same OSB references in the picker, reader, plans, bookmarks, links and
   read-aloud highlighting, regardless of the selected translation.
3. Explicit handling of split, merged, reordered and lettered verses, and of text
   absent from a source. Never fill a missing passage with a guessed same-number
   lookup or present another source's words as SRB.
4. Compatibility for saved readings, bookmarks, resume positions and plan progress
   created with the previous frames. Preserve their original coordinate system so
   that changing the display frame cannot silently move the saved passage.
5. Regression checks for the cases above and other reviewed exceptions, plus
   end-to-end checks that the rendered and spoken words come from the same source.

The OSB EPUB and its text remain local reference inputs. The runtime SRB importer
continues to require the two publisher PDFs; possession of OSB must not become a
requirement for importing or reading SRB.
