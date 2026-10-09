# SQLite storage policy

The native application uses five SQLite databases. It does not use platform preference defaults as its storage system.

`corpus.db` contains Scripture, source metadata, alignment, pronunciation, and recurring reading rules.
The connection opens with `SQLITE_OPEN_READONLY` and enables `query_only`.
Its schema version is 4, and its application identifier is `ORTC`.
Application tables use `STRICT` typing. Foreign keys connect reading segments to their rules and books.
The book and canon tables come from `books.tsv` and `canon.tsv`: names, abbreviations, testament, deuterocanonical and stanza flags, and the reader's book order.
The application loads them once when the corpus opens.
The paragraph table records USFM boundaries and labelled WEB editorial boundaries for JSON editions.
The verse uniqueness index also serves coordinate lookups. The importer runs `ANALYZE` before packaging.
The read cache allows 8 MiB. The shipped file needs no writable journal or companion files.

`study.db` contains the word-study data from `tools/lexicon/build_study.py`.
It is read-only, like the corpus. Its schema version is 3, and its application identifier is `ORST`.
The reader works without it; the word-study panel then stays empty.

`user.db` contains preferences and reading progress in the platform user-data directory.
It has no schema versions or migrations until Perikop has users. Opening it creates any missing table as the current code defines it.
The `progress` table stores a key, such as `plan:nt:7`, and the local date it was marked as read.

The settings connection explicitly enables WAL journaling and `synchronous=FULL`.
It checkpoints after 64 pages and limits retained journal size to 256 KiB.
On macOS, commits and checkpoints also enable full filesystem synchronization.
The settings read cache allows 256 KiB. Each save updates all preferences in one immediate transaction.
If a save fails, rollback preserves the previous values and the original error.

`speech-cache.db` stores completed 24 kHz mono chunks as little-endian float PCM.
Its schema version is 1, and its application identifier is `ORTS`.
The key includes the pinned model, reference voice, normalization version, synthesis settings, language, and exact speech text.
WAL journaling and `synchronous=NORMAL` permit regeneration after an interrupted write.
The read cache allows 2 MiB. It checkpoints after 256 pages.
At most 256 MiB of audio remains in the table. SQLite reuses evicted pages.
Transactions update each chunk and evict the oldest entries when needed.
Invalid samples and unknown future schemas are rejected.
One speech worker owns the cache connection.

`pronunciation-review.db` contains the decisions of the Swedish pronunciation review.
Its schema version is 1, and its application identifier is `ORTP`.
Corrected forms override the corpus pronunciations when the reader speaks Swedish.
The [pronunciation review guide](pronunciation-review.md) describes the tool.

All connections enable foreign keys, defensive mode, and extended result codes.
They disable trusted schemas and wait up to three seconds for temporary database locks.
SQL statements bind values and check binding errors. No text input becomes SQL syntax.
Loadable SQLite extensions remain disabled in the build.

The corpus builder creates a temporary database, checks integrity and foreign keys, then atomically replaces its output.
It explicitly uses 4096-byte pages, DELETE journaling, and full synchronization during this offline import.
The manifest records the resulting file hash and each input hash.
The importer rejects unknown Scripture books and unsupported verse labels.

Tests cover preference persistence, reading progress, and rollback after an interrupted save.
They also check read-only corpus writes, complete edition endpoints, lettered verses, and joined verse ranges.

These policies follow the [SQLite PRAGMA documentation](https://sqlite.org/pragma.html) and the connection configuration API.
