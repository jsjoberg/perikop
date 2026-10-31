#!/usr/bin/env -S uv run --locked
"""Check guarded 1 Kings corrections, retained readings and saved image evidence."""
import hashlib
import json
from pathlib import Path
import sqlite3
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/corpus'))
import runeberg1917
from check_swedish_verification import text_hash, validate
from sv1917_1kings import REPLACE

report = json.loads((ROOT / 'docs/reports/sv1917-verification-1kings.json').read_text())
ledger = json.loads((ROOT / 'docs/reports/sv1917-verification.json').read_text())
source = ROOT / 'resources/corpus/input/runeberg-bibeln-1917-html.zip'
corrected = runeberg1917.read(source)
for key in REPLACE:
    del runeberg1917.REPLACE[key]
try:
    original = runeberg1917.read(source)
finally:
    runeberg1917.REPLACE.update(REPLACE)

assert len(REPLACE) == len(report['corrections']) == 218
assert set(REPLACE) == {key for key in original if original[key] != corrected[key]}
for correction in report['corrections']:
    key = ('1Kgs', correction['chapter'], correction['verse'])
    assert original[key] == correction['before']
    assert corrected[key] == correction['after']
    assert REPLACE[key] == (correction['guard_before'], correction['guard_after'])

# A moved or ambiguous source span must stop the import instead of silently patching it.
key = next(iter(REPLACE))
for invalid in ('missing source span', ' '):
    runeberg1917.REPLACE[key] = (invalid, 'replacement')
    try:
        runeberg1917.read(source)
    except ValueError as error:
        assert 'does not apply once' in str(error)
    else:
        raise AssertionError('A missing or repeated correction span was accepted')
    finally:
        runeberg1917.REPLACE[key] = REPLACE[key]

with sqlite3.connect((ROOT / 'resources/corpus/corpus.db').as_uri() + '?mode=ro', uri=True) as db:
    rows = db.execute("""SELECT b.code,v.chapter,v.verse,v.text FROM verse v
        JOIN book b ON b.id=v.book_id JOIN source s ON s.id=v.source_id
        WHERE s.code='sv1917' AND b.code='1Kgs' ORDER BY v.chapter,v.verse""").fetchall()
assert len(rows) == 817
assert {tuple(row[:3]): row[3] for row in rows} == {
    key: value for key, value in corrected.items() if key[0] == '1Kgs'}
chapters = {('1Kgs', chapter): [row for row in rows if row[1] == chapter]
            for chapter in range(1, 23)}
assert validate(report['chapters'], chapters) == {('1Kgs', c) for c in range(1, 23) if c != 14}
assert report['chapters'] == [c for c in ledger['chapters'] if c['book'] == '1Kgs']
for chapter in report['chapters']:
    assert chapter['text_sha256'] == text_hash(chapters['1Kgs', chapter['chapter']])
assert len(report['unresolved']) == 1
assert report['unresolved'][0]['id'] == 'KGS1-V001'
assert 'synder som' in corrected['1Kgs', 14, 22]
assert corrected['1Kgs', 8, 41].endswith(' –')
assert corrected['1Kgs', 8, 42].startswith('ty man')
assert corrected['1Kgs', 12, 33].startswith('Till det altare')
assert 'den som omgjordar' in corrected['1Kgs', 20, 11]
assert 'och såsom det var skrivet i brevet' in corrected['1Kgs', 21, 11]
assert 'gjorde, det' in corrected['1Kgs', 15, 31]

witness_dir = ROOT / 'docs/reports/sv1917-1kings-witnesses'
manifest = json.loads((witness_dir / 'manifest.json').read_text())
assert len(manifest) == len({m['path'] for m in manifest}) == 94
for item in manifest:
    path = (ROOT / item['path']).resolve()
    assert path.is_relative_to(witness_dir)
    assert hashlib.sha256(path.read_bytes()).hexdigest() == item['sha256'], item['path']
scans = {(m['url'], m['sha256']) for m in manifest if m['edition'] == 'efs1927'}
assert len(scans) == 46
for chapter in report['chapters']:
    assert all((s['url'], s['sha256']) in scans for s in chapter['scans'])
paths = {str((ROOT / m['path']).relative_to(witness_dir)) for m in manifest}
for question in report['unresolved'] + report['resolved']:
    assert set(question['evidence']) <= paths
print('1 Kings: 218 guarded corrections, 817 verses, 22 chapter hashes and 94 image files passed')
