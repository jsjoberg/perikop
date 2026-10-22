# Reading-plan ranges and compatibility

The fixed version-3 plans use the [OSB reference authority](osb-reference.md).
Book order and chapter divisions follow OSB, independently of the preferred text.
Word estimates use Swedish 1917 where available and English for missing material.

| Plan | Parts | Mean words | Range |
| --- | ---: | ---: | ---: |
| New Testament | 60 | 3,019 | 2,131–4,139 |
| Old Testament | 182 | 3,070 | 1,390–4,543 |
| Other Septuagint books | 36 | 3,336 | 2,124–4,392 |

The OT plan includes Manasseh at 2 Chronicles 37 and Psalm 151. The other plan now
includes 1 Ezra and 3 Maccabees. Books lacking Swedish text open with an enabled
English or Greek source; those passages have no Swedish read-aloud.

| NT section | Parts |
| --- | --- |
| Matthew | 1–8 |
| Mark | 9–13 |
| Luke | 14–22 |
| John | 23–28 |
| Acts | 29–36 |
| Letters | 37–56 |
| Revelation | 57–60 |

Every part ends at a chapter boundary. A part containing several books contains
those books in full. Gospels, Acts and Revelation never share a part with another
book. Short letters remain whole; Romans, 1–2 Corinthians and Hebrews need splits.
Whole chapters explain unavoidable variation: OSB 1 Kingdoms 1 has about 1,390
words, while its expanded chapter 2 has about 4,543, so they cannot share one part.

`tools/build_reading_plans.py` counts each contiguous translation unit once.
Repeated material at separate OSB locations counts at each occurrence. It minimizes
variation around the NT mean, with exactly 60 NT parts and freely chosen counts for
the other plans. Generated ranges compile from `src/core/reading_plan_parts.inc`;
changing the user's edition never recalculates them.

```sh
python tools/build_reading_plans.py --check
```

Use `--write` only when deliberately editing the layout. Published boundary changes
require another key version and migration, even when the number of parts is unchanged.

## Compatibility

Startup calls `UserDb::migrate_reading_plans`. The original 30/30/14-part layout is
frozen in `legacy_reading_plan_parts.inc`; the previous 60/180/31-part layout is
frozen in `reading_plan_v2_parts.inc`. The old source canon and frames remain
available only for conversion. Neither frozen layout is regenerated from OSB data.

1. `reading_plan_migration.cpp` transfers v1 coverage into v2 keys in its own SQLite
   transaction, with the `reading_plans_v2_migrated` marker.
2. `osb_migration.cpp` transfers v2 coverage and ordinary reading checkpoints into
   OSB v3 keys and `reading:osb-v1:...` keys in a second transaction, with the
   `osb_v1_migrated` marker.

Completed old parts contribute their original framed verses. A valid checkpoint
contributes only verses before its current verse, which is read again on resume.
A new verse is covered only when all its old source counterparts are covered.
New OSB material without old coverage remains unread. Nonconsecutive completions
are handled independently; the first unread gap determines the new checkpoint.

Completion dates use the latest contributing date. Existing OSB completion or
checkpoint rows take precedence. Original progress and positions remain as recovery
data; settings, bookmarks and unrelated day progress are retained. Each phase rolls
back its writes and marker on failure and can retry on the next launch.

**Börja om** clears the plan's old and new keys. Both migration markers remain,
so reset cannot resurrect cleared progress. The old converters and frozen tables
can eventually be retired, but the v3 key format and published ranges must remain.
