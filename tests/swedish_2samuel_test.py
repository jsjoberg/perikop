#!/usr/bin/env -S uv run --locked
"""Detect stale 2 Samuel corpus/scan evidence; this cannot certify image reading."""
import collections
import hashlib
import json
from pathlib import Path
import sqlite3
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/corpus'))
from check_swedish_verification import validate
from runeberg1917 import read, REPLACE

report = json.loads((ROOT / 'docs/reports/sv1917-verification-2samuel.json').read_text())
ledger = json.loads((ROOT / 'docs/reports/sv1917-verification.json').read_text())
with sqlite3.connect((ROOT / 'resources/corpus/corpus.db').as_uri() + '?mode=ro', uri=True) as db:
    rows = db.execute("""SELECT b.code,v.chapter,v.verse,v.text FROM verse v
        JOIN book b ON b.id=v.book_id JOIN source s ON s.id=v.source_id
        WHERE b.code='2Sam' AND s.code='sv1917' ORDER BY chapter,verse""").fetchall()
chapters = collections.defaultdict(list)
for row in rows:
    chapters[row[:2]].append(row)
assert len(rows) == 695 and len(chapters) == 24
assert validate(report['chapters'], chapters) == set(chapters)
assert [r for r in ledger['chapters'] if r['book'] == '2Sam'] == report['chapters']
assert report['totals'] == dict(visually_compared_chapters=24, visually_compared_verses=695,
                              verified_chapters=24, verified_verses=695, corrected_verses=210)
assert report['unresolved'] == []
manifest = json.loads((ROOT / 'docs/reports/sv1917-2samuel-witnesses/manifest.json').read_text())
assert len(manifest) == 77
by_url = {e['url']: e for e in manifest}
for e in manifest:
    assert hashlib.sha256((ROOT / e['path']).read_bytes()).hexdigest() == e['sha256']
for chapter in report['chapters']:
    for e in chapter['scans'] + chapter['secondary_scans']:
        assert by_url[e['url']]['sha256'] == e['sha256']
imported = read(ROOT / 'resources/corpus/input/runeberg-bibeln-1917-html.zip')
actual = {(b,c,v):t for b,c,v,t in rows}
assert {k:v for k,v in imported.items() if k[0] == '2Sam'} == actual
assert len(report['corrections']) == 210
assert len({(e['chapter'], e['verse']) for e in report['corrections']}) == 210
for i,e in enumerate(report['corrections'],1):
    key = '2Sam', e['chapter'], e['verse']
    assert e['id'] == f'SAM2-{i:03}'
    assert REPLACE[key] == (e['old'],e['new']) and actual[key] == e['new']
    for field,text_field in [('old_verse_sha256','old'),('new_verse_sha256','new')]:
        assert hashlib.sha256(e[text_field].encode()).hexdigest() == e[field]
assert 'med Abigal,' in actual['2Sam',3,3]
assert 'broder Aminon' in actual['2Sam',13,20]
assert 'Jordanslätten' in actual['2Sam',18,23]
assert [v for b,c,v,t in rows if c==24] == list(range(1,26))
errata = (ROOT / 'docs/reports/sv1917-errata-2samuel.md').read_text()
assert errata.count('| [ ] | SAM2-') == 210 and '| [x] | SAM2-' not in errata
print('PASS: 24 chapters/695 verses; 210 guards; 77 source hashes; retained variants and numbering.')
