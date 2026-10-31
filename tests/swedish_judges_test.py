#!/usr/bin/env -S uv run --locked
"""Judges image evidence, corrected omissions, and intentionally retained print variants."""
import hashlib
import json
import sqlite3
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/corpus'))
from check_swedish_verification import validate
from runeberg1917 import read
from sv1917_judges import REPLACE

manifest = json.loads((ROOT / 'docs/reports/sv1917-judges-witnesses/manifest.json').read_text())
assert len(manifest) == 70
by_url = {item['url']: item for item in manifest}
assert len(by_url) == 70
for item in manifest:
    path = (ROOT / item['path']).resolve()
    assert path.is_relative_to(ROOT.resolve())
    assert hashlib.sha256(path.read_bytes()).hexdigest() == item['sha256'], item['path']
assert {x['page'] for x in manifest if x['edition'] == 'efs1927'} == set(range(333, 370))
assert {x['leaf'] for x in manifest if x['edition'] == 'efs1926'} == set(range(316, 349))

with sqlite3.connect((ROOT / 'resources/corpus/corpus.db').as_uri() + '?mode=ro', uri=True) as db:
    rows = db.execute("""SELECT b.code,v.chapter,v.verse,v.text FROM verse v
        JOIN source s ON s.id=v.source_id JOIN book b ON b.id=v.book_id
        WHERE s.code='sv1917' AND b.code='Judg' ORDER BY v.chapter,v.verse""").fetchall()
verses = {(b,c,v): t for b,c,v,t in rows}
assert len(verses) == 618 and len(REPLACE) == 168
assert all(key[0] == 'Judg' for key in REPLACE)
imported = read(ROOT / 'resources/corpus/input/runeberg-bibeln-1917-html.zip')
assert verses == {key: value for key, value in imported.items() if key[0] == 'Judg'}
chapters = {('Judg', c): [row for row in rows if row[1] == c] for c in range(1,22)}
records = [r for r in json.loads((ROOT / 'docs/reports/sv1917-verification.json').read_text())['chapters'] if r['book']=='Judg']
assert validate(records, chapters) == set(chapters)
for record in records:
    assert record['supplementary_scans']
    for scan in record['scans'] + record['supplementary_scans']:
        assert scan['sha256'] == by_url[scan['url']]['sha256']
        assert scan['path'] == by_url[scan['url']]['path']

for c,v,text in [(1,21,'ännu i dag.'),(2,9,'Timnat-Heres'),(3,4,'deras fäder genom Mose.'),
                 (3,27,'i basun i Efraims'),(4,19,'mjölkkärlet'),(5,1,'Då sjöngo'),
                 (5,17,'Och Dan – varför dröjer'),(6,16,'såsom vore det'),(6,27,'HERREN hade sagt'),
                 (11,10,'bliva, såsom du'),(13,5,'göra en begynnelse'),(13,12,'Huru skall man'),
                 (15,15,'tusen man.'),(20,34,'tio tusen män,'),(21,8,'Och de frågade'),(21,19,'En HERRENS högtid')]:
    assert text in verses['Judg',c,v], (c,v,text)
# An edition-supported reading must survive the corrections and future rebuilds.
for c,v,text in [(1,3,'sedan tåga'),(2,2,'deras altaren.'),(4,21,'sin hand, gick'),
                 (5,14,'Ned ifrån Makir'),(6,34,'HERRENS Andes kraft'),(7,13,'så liggande'),
                 (7,19,'Så kommo'),(10,15,'rädda oss nu denna'),(14,17,'på gåtan åt'),
                 (16,10,'huru man'),(16,29,'mittelpelare'),(18,14,'som hade varit'),
                 (18,22,'av de män'),(19,21,'tvått sina fötter'),(20,33,'Baal-Tamar'),
                 (21,5,'icke komme upp'),(21,11,'som hava haft'),(21,22,'oss dem; ty')]:
    assert text in verses['Judg',c,v], (c,v,text)
errata = (ROOT / 'docs/reports/sv1917-errata-judges.md').read_text()
assert errata.count('| [ ] |') == 168 and '| [x] |' not in errata
print('Judges: 21 chapters, 618 verses, 168 correction records, 70 exact scan files passed')
