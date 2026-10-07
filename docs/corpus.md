# Offline corpus

## Bundled editions

Perikop bundles 141,201 Scripture records from each publisher's complete available edition.

| Edition | Records |
| --- | ---: |
| Swedish 1917 Bible and 1921 apocrypha | 35,515 |
| Greek Old Testament | 28,597 |
| Greek New Testament | 7,958 |
| King James Version | 31,102 |
| World English Bible | 38,029 |

The World English Bible includes deuterocanonical books.
The Bible browser offers it as a separate edition and lists only books available in the selected edition.
No restricted modern Swedish translation is bundled.

Each source retains its wording and chapter and verse coordinates.
Lettered Greek coordinates, such as Genesis 31:50a, remain separate records.
Joined publisher verses retain their complete ranges.
The Greek Psalms retain LXX numbering.

An explicit alignment links Swedish, Greek, and English verses across their numbering systems.
Verses without a counterpart show a message.
See the [versification guide](versification.md) for source corrections, alignment rules, and displayed coordinates.
Word-level MT/LXX alignment remains open.

## Storage

`resources/corpus/corpus.db` opens in read-only mode.
`user.db` resides in the platform user-data directory.
All SQL stays in the storage layer.
The [storage guide](storage.md) describes the explicit SQLite policy.

## Rebuild and check resources

Rebuild the corpus from bundled source data:

```sh
uv run --locked tools/corpus/build_corpus.py
```

Python is a corpus development tool.
The native build, packaged application, and application startup do not need Python or `uv`.
`pyproject.toml` and `uv.lock` define the preparation environment.
Voice export uses the separate `voice-prep` dependency group.
Normal resource tools use only the Python standard library.

Check the resource hashes and complete corpus glyph coverage:

```sh
uv run --locked tools/check_resources.py
```

`resources/manifest.json` records resource hashes and provenance.
[Third-party notices](../THIRD-PARTY-NOTICES.md) lists licenses and edition limits.

## Display metadata and limits

Prose uses bundled USFM paragraph boundaries.
Swedish and KJV use WEB boundaries as editorial display metadata where coordinates agree.
Poetry retains verse stanzas. Paragraph breaks inside a single verse are not yet retained.
The [architecture guide](architecture.md) describes native paragraph layout and its cache.
