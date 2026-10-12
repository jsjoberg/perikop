# Offline corpus

## Bundled editions

Perikop bundles 108,347 Scripture records from each publisher's complete available edition.

| Edition | Records |
| --- | ---: |
| Swedish 1917 Bible and 1921 apocrypha | 35,515 |
| Greek Old Testament | 28,597 |
| Greek New Testament | 7,958 |
| LXX2012, English Old Testament | 28,324 |
| World English Bible, New Testament | 7,953 |

The English edition pairs LXX2012 with the World English Bible New Testament, as Orthocal does.
LXX2012 is Brenton's 1851 translation of the Septuagint with American English updates.
English and Greek therefore have the same Old Testament books. Only the Swedish edition lacks some of them.
No restricted modern Swedish translation is bundled.

Each source retains its wording and chapter and verse coordinates.
Lettered Greek coordinates, such as Genesis 31:50a, remain separate records.
Joined publisher verses retain their complete ranges.
The Greek Psalms retain LXX numbering.

An explicit alignment links Swedish, Greek, and English verses across their numbering systems.
LXX2012 translates the Greek text, but follows English chapter divisions in places, such as Joel 3 and Malachi 4.
Verses without a counterpart show a message.
See the [versification guide](versification.md) for source corrections, alignment rules, and displayed coordinates.
Word-level MT/LXX alignment remains open.

## Storage

`resources/corpus/corpus.db` opens in read-only mode.
`user.db` resides in the platform user-data directory.
All SQL stays in the storage layer.
The [storage guide](storage.md) describes the explicit SQLite policy.

## Build witnesses

The corpus build also reads three public-domain editions that do not ship:

- The KJV states the numbering of Orthocal's lectionary references. The build restates each reference in Septuagint or Patriarchal numbering.
- The complete World English Bible supplies Swedish paragraph boundaries.
- Brenton's English Septuagint has the Greek numbering. Shared wording links each LXX2012 verse to its Greek verse.

The build writes all editions to `build/corpus/work.db`, which `align_editions.py` reads.
`resources/corpus/corpus.db` omits the witnesses.

## Rebuild and check resources

Rebuild the corpus from bundled source data:

```sh
uv run --locked tools/corpus/build_corpus.py
```

After a change to the inputs or alignment rules, run `tools/corpus/align_editions.py` and then rebuild again.

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
Swedish uses WEB boundaries as editorial display metadata where coordinates agree.
LXX2012 marks only chapters and poetry, so it takes the Greek boundaries through the alignment.
Poetry retains verse stanzas. Paragraph breaks inside a single verse are not yet retained.
The [architecture guide](architecture.md) describes native paragraph layout and its cache.
