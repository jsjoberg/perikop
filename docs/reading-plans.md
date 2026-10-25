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

Use `--write` only when deliberately editing the layout. A published boundary change
needs a new key version in `plan_key()`, even when the number of parts is unchanged.

## Compatibility

Progress keys carry the layout version, such as `plan:nt:v4:7`. Marks and positions
saved for an earlier layout are not converted: they stay in the profile unused, and
the plan starts over. Version 4 began with 0.2.0, together with `passage:` resume keys
(shared with daily readings of the same passage), so no earlier position applies either. Daily-reading marks, bookmarks and
settings are kept separately and are unaffected, except the 0.1.x split-pane setting
(`parallel`), which is no longer read.

**Börja om** clears every version's marks for the plan, and its reading positions.
