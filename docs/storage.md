# SQLite storage policy

The native application uses two SQLite databases. It does not use platform preference defaults as its storage system.

`corpus.db` contains Scripture, source metadata, alignment, pronunciation, and recurring reading rules.
The connection opens with `SQLITE_OPEN_READONLY` and enables `query_only`.
Its schema version is 2, and its application identifier is `ORTC`.
Application tables use `STRICT` typing. Foreign keys connect reading segments to their rules and books.
The verse uniqueness index also serves coordinate lookups. The importer runs `ANALYZE` before packaging.
The read cache allows 8 MiB. The shipped file needs no writable journal or companion files.

`user.db` contains preferences in the platform user-data directory.
Its schema version is 1, and its application identifier is `ORTU`.
A transaction migrates the original unversioned settings table without discarding values.
Unknown future versions and foreign application identifiers cause an error.

The settings connection explicitly enables WAL journaling and `synchronous=FULL`.
It checkpoints after 64 pages and limits retained journal size to 256 KiB.
On macOS, commits and checkpoints also enable full filesystem synchronization.
The settings read cache allows 256 KiB. Each save updates all preferences in one immediate transaction.
If a save fails, rollback preserves the previous values and the original error.

Both connections enable foreign keys, defensive mode, and extended result codes.
They disable trusted schemas and wait up to three seconds for temporary database locks.
SQL statements bind values and check binding errors. No text input becomes SQL syntax.
Loadable SQLite extensions remain disabled in the build.

The corpus builder creates a temporary database, checks integrity and foreign keys, then atomically replaces its output.
It explicitly uses 4096-byte pages, DELETE journaling, and full synchronization during this offline import.
The manifest records the resulting file hash and each input hash.
The importer rejects unknown Scripture books and unsupported verse labels.

Tests cover preference migration, persistence, future-version rejection, and rollback after an interrupted save.
They also check read-only corpus writes, complete edition endpoints, lettered verses, and joined verse ranges.

These policies follow the [SQLite PRAGMA documentation](https://sqlite.org/pragma.html) and the connection configuration API.
