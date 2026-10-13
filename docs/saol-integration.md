# SAOL 15 open word data: integration requirements

Status on 2026-10-12: awaiting an authorized dataset export.
SAOL data is not bundled or displayed by Perikop yet.
SALDO is the integrated alternative. See [Word study](word-study.md) for their scope and licences.

## Verified source and licence

[SAOL 15 (2026) – ord](https://www.sprakbanken.se/resurser/saol15-ord)
contains 128,295 entries according to Språkbanken.
The creator is Svenska Akademien. Språkbanken processes and distributes the dataset.
The published dataset date is 2026-05-21.
Its persistent identifier is [doi:10.23695/eqtv-ee68](https://doi.org/10.23695/eqtv-ee68).

The resource page and the
[upstream metadata](https://github.com/spraakbanken/metadata/blob/main/yaml/lexicon/saol15-ord.yaml)
identify the licence as [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
This licence permits reuse and redistribution for commercial purposes.
The [legal code](https://creativecommons.org/licenses/by/4.0/legalcode.en)
requires attribution, a licence reference, and a statement of modifications.
Supplied copyright notices, disclaimer notices, and earlier modification statements must remain available.
Redistribution must not add restrictions to the licensed data or suggest endorsement.

This licence applies to the specified open word dataset.
It does not establish permission to copy dictionary definitions from svenska.se.

## Access obstacle

Språkbanken lists a [Karp interface](https://spraakbanken.gu.se/karp?resources=saol15-ord), but no downloadable files.
The interface uses the public Karp search API at
`https://spraakbanken4.it.gu.se/karps/v2-dev_1/`.

The API's `/config` response lists `saol15-ord` with `limitedAccess: true`.
The [API specification](https://spraakbanken4.it.gu.se/karps/v2-dev_1/openapi.json)
defines this flag as access restricted to authorized users.
An unauthenticated exact-word search returned:

```json
{
  "message": "user does not have access to resource: saol15-ord",
  "code": 2,
  "details": {"resource": "saol15-ord"}
}
```

An authorized export of this specific resource is required before implementation and data verification can finish.
The contact listed by Språkbanken is `sb-info@svenska.gu.se`.
No access controls were bypassed and no restricted dictionary content was downloaded.

## Import and matching requirements

The export must identify `saol15-ord`, its dataset revision, and its applicable licence.
The input catalog must record its source, attribution, licence URL, and SHA-256 hash.
Keep the original export and its supplied notices available during the import review.
Use only the open dataset's headwords, word classes, homograph numbers, related words, and source links.
Do not fetch linked dictionary pages during import or application lookup.

Match whole words, with Unicode normalization and case normalization.
Keep separate homographs and their word classes and source links.
Use published relationships or reviewed mappings for inflected and historical forms.
Do not reuse Dalin's suffix guesses as evidence of a SAOL match.
Distinguish exact matches from mapped headwords in the displayed result.
Actual export samples must determine the parser's field types and relationship semantics.

The application must keep lookups offline and show SAOL only when a word has a verified match.
The SAOL section must complement Dalin and Nyström in Swedish word study.
Show the headword, word class, homograph number when present, and an external source link.
Label the section as word data, rather than dictionary definitions.
Source links must use HTTPS and the expected svenska.se host.

## Attribution and release requirements

The SAOL section must credit Svenska Akademien and Språkbanken and link to the dataset and licence.
The About dialog and third-party notices must retain the full dataset citation and supplied notices.
The resource manifest must identify the dataset version and licence separately from the application code.
Record the actual changes: filtering, normalization, lookup mappings, and conversion to SQLite.
Do not claim transformations or coverage until the real data is imported and measured.

Before release, verify the input hash and all bundled resource hashes.
Repeat the database build and compare its output hash.
Exercise exact matches, Unicode words, homographs, mapped forms, absent words, and invalid source URLs.
Run the core tests and UI smoke tests with the bundled dataset.
Verify attribution links and matched-only display in the word-study panel.
