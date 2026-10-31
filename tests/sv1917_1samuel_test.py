#!/usr/bin/env -S uv run --locked
"""Retained images, exact reviewed chapters, and Swedish numbering must remain intact."""
import hashlib
import json
import sqlite3
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/corpus'))
from check_swedish_verification import validate
from mapping import Mapper
import runeberg1917

ledger = json.loads((ROOT / 'docs/reports/sv1917-verification.json').read_text())
records = [row for row in ledger['chapters'] if row['book'] == '1Sam']
assert len(records) == 31 and all(row['status'] == 'verified' for row in records)
with sqlite3.connect((ROOT / 'resources/corpus/corpus.db').as_uri() + '?mode=ro', uri=True) as db:
    rows = db.execute("""SELECT b.code, v.chapter, v.verse, v.text FROM verse v
        JOIN book b ON b.id=v.book_id JOIN source s ON s.id=v.source_id
        WHERE s.code='sv1917' AND b.code='1Sam' ORDER BY v.chapter,v.verse""").fetchall()
    chapters = {('1Sam', ch): [row for row in rows if row[1] == ch] for ch in range(1, 32)}
    assert len(rows) == 811 and len(chapters['1Sam', 3]) == 21
    assert validate(records, chapters) == set(chapters)
    imported = runeberg1917.read(ROOT / 'resources/corpus/input/runeberg-bibeln-1917-html.zip')
    assert {key: text for key, text in imported.items() if key[0] == '1Sam'} == {
        (book, ch, verse): text for book, ch, verse, text in rows}
    assert ('1Sam', 3, 22) not in imported
    assert imported['1Sam', 4, 1].startswith('Och Samuels ord kom till hela Israel. Och Israel drog ut')
    assert 'runt omkring honom.' in imported['1Sam', 26, 5]
    assert 'med med oss' in imported['1Sam', 29, 4]
    assert 'och fick se, att' in imported['1Sam', 30, 3]
    assert imported['1Sam', 28, 2].startswith('David svarade Akis:')
    mapper = Mapper(db)
    for source in ('grc-lxx', 'en-lxx2012'):
        for a, b in (('sv1917', source), (source, 'sv1917')):
            for ref in ((3, 21, ''), (4, 1, ''), (4, 2, '')):
                assert mapper.counterparts(a, b, '1Sam', ref) == [('1Sam', ref)]
    # These Swedish verses keep their separate printed numbers and existing counterparts.
    assert mapper.counterparts('sv1917', 'grc-lxx', '1Sam', (20, 43, '')) == [('1Sam', (21, 1, ''))]
    assert mapper.counterparts('sv1917', 'grc-lxx', '1Sam', (24, 1, '')) == [('1Sam', (24, 1, ''))]

manifest = json.loads((ROOT / 'docs/reports/sv1917-1samuel-witnesses/manifest.json').read_text())
images = manifest['efs1927'] + manifest['efs1926']
assert len(manifest['efs1927']) == 48 and len(manifest['efs1926']) == 44
by_path = {scan['path']: scan for scan in images}
assert len(by_path) == 92
for scan in images:
    path = ROOT / scan['path']
    assert path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == scan['sha256'], path
for record in records:
    for scan in record['scans'] + record['supplementary_scans']:
        assert scan == by_path[scan['path']]
errata = (ROOT / 'docs/reports/sv1917-errata.md').read_text().split('## Första Samuelsboken:')[1]
assert errata.count('| [ ] |') == 267 and '| [x] |' not in errata
print('1 Samuel: 31 chapters, 811 verses, 92 retained images and numbering/mappings passed')
