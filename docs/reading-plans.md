# Reading-plan ranges and compatibility

The three plans use fixed version-2 ranges. They preserve the previous book order and coverage, including Septuagint chapter numbering and the omission of books without Swedish text. The word counts below are the generator's estimates from the bundled Swedish text.

| Plan | Parts | Mean Swedish words | Range |
| --- | ---: | ---: | ---: |
| New Testament | 60 | 3,016 | 2,131–4,139 |
| Old Testament | 180 | 3,057 | 1,728–3,890 |
| Other Septuagint books | 31 | 3,294 | 384–4,400 |

The 384-word exception is the complete Prayer of Manasseh. Giving it its own part avoids attaching it to only the beginning of Tobit.

| NT section | Parts |
| --- | --- |
| Matthew | 1–8 |
| Mark | 9–13 |
| Luke | 14–22 |
| John | 23–28 |
| Acts | 29–36 |
| Letters | 37–56 |
| Revelation | 57–60 |

Every part ends at a chapter boundary. Parts containing more than one book contain those books in full. Books that fit within 1.6 times the NT target length stay whole. Larger books split into chapter ranges; an extra cost for cuts inside letters favors fewer, slightly longer parts. Romans, 1–2 Corinthians, and Hebrews need splits; the shorter NT letters remain whole.

`tools/build_reading_plans.py` counts the bundled Swedish text, using the reader's Septuagint-to-Swedish mappings and counting merged counterparts once. It minimizes variation from the NT's mean length, with exactly 60 NT parts and freely chosen counts for the other plans. The generated ranges are compiled from `src/core/reading_plan_parts.inc`; the application never recalculates them from the user's chosen edition.

```sh
uv run --locked tools/build_reading_plans.py --check
```

Use `--write` only when deliberately editing the layout. Moving published boundaries requires a new progress-key version and a migration, even when the part count is unchanged.

## Migration from the original 30/30/14-part plans

Normal application startup calls `UserDb::migrate_reading_plans` once per profile. A single SQLite transaction transfers progress and records the `reading_plans_v2_migrated` marker. Failures roll back both the new rows and the marker, so the next launch can retry.

- New keys such as `plan:nt:v2:7` cannot be confused with old keys such as `plan:nt:7`.
- Completed old parts contribute all their framed verses. A valid checkpoint contributes the verses before the saved current verse, which is read again when resuming. If no plan checkpoint exists, the old reading's ordinary checkpoint is used.
- A new part is complete only when all its verses are covered. Its date is the latest contributing completion date; text inferred from a checkpoint uses the migration date.
- An incomplete part with known progress resumes at its first unread verse, with its section and percentage recalculated for the new range. This also handles Nehemiah's offset within Greek Ezra and Greek Esther's separate frame book.
- Nonconsecutive old completions are handled independently. At a new boundary, already-read text after an unread gap may be shown again; the migration never skips the gap or marks unread text complete.
- Existing v2 progress takes precedence. Legacy completion rows and checkpoints remain as recovery data, while unrelated settings, bookmarks, and day progress stay intact.
- **Börja om** clears both old and new plan keys. The migration marker stays, so an intentional reset cannot be undone by a later launch.

The compatibility code is isolated in `src/storage/reading_plan_migration.cpp` and its frozen `legacy_reading_plan_parts.inc`. To retire support for the original layout, remove that code, its startup call, declaration, migration-specific tests, and CMake source entry. Keep the v2 key format and fixed ranges: old profiles will then start these plans without converted progress, rather than applying old ordinal numbers to new readings.
