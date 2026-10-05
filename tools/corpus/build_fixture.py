#!/usr/bin/env python3
"""Rebuild the shipped fixture database using only bundled data (offline)."""
import hashlib
import json
from pathlib import Path
import sqlite3
import sys
root = Path(__file__).resolve().parents[2]
output = Path(sys.argv[1]) if len(sys.argv) > 1 else root / 'resources/corpus/corpus.db'
data = json.loads((root / 'resources/corpus/fixture.json').read_text())
output.parent.mkdir(parents=True, exist_ok=True)
temporary = output.with_suffix('.tmp')
temporary.unlink(missing_ok=True)
with sqlite3.connect(temporary) as db:
    db.executescript((root / 'resources/corpus/schema.sql').read_text())
    for source in data['sources']:
        db.execute('INSERT INTO source VALUES (:id,:code,:language,:name,:versification,:license_id,:attribution)', source)
    for book in data['books']:
        db.execute('INSERT INTO book VALUES (:id,:code,:canonical_order,:name_sv,:name_el,:name_en)', book)
    db.executemany('INSERT INTO verse(source_id,book_id,chapter,verse,text) VALUES(?,?,?,?,?)', data['verses'])
    # Curated mappings, including title and split-verse differences in Psalm 22.
    def mapping(src, dst, fc, fv, flv, tc, tv, tlv, kind=1):
        db.execute('INSERT INTO alignment VALUES(?,?,1,?,?,?,?,?,?,?,?,?)',
                   (src,dst,fc,fv,fc,flv,tc,tv,tc,tlv,kind))
    for mt,lxx,length in [(23,22,6),(24,23,10),(25,24,22)]:
        mapping(1,2,mt,1,length,lxx,1,length)
        mapping(4,2,mt,1,length,lxx,1,length)
    mapping(1,2,22,1,30,21,1,30)
    mapping(1,2,22,31,31,21,31,32,2)  # one Swedish verse spans two Greek verses
    mapping(1,4,22,1,1,22,1,1,3)     # title-only Swedish row needs merged treatment
    mapping(1,4,22,2,30,22,1,29)
    mapping(1,4,22,31,31,22,30,31,2)
    mapping(4,2,22,1,29,21,2,30)
    mapping(4,2,22,30,31,21,31,32)
    db.executemany('INSERT INTO pronunciation VALUES(?,?,?,?,?)', [
        ('sv','Melkisedek','Melki-sedek','',100),
        ('sv','Lukasevangeliet','Lukas evangelium','',100),
        ('sv','Filipperbrevet','Filipper brevet','',100)])
    db.execute('PRAGMA user_version=1')
    assert db.execute('PRAGMA integrity_check').fetchone()[0] == 'ok'
    db.commit()
temporary.replace(output)
manifest_path = root / 'resources/manifest.json'
manifest = json.loads(manifest_path.read_text())
manifest['assets'] = [a for a in manifest['assets'] if a.get('path') != 'resources/corpus/corpus.db']
manifest['assets'].append(dict(path='resources/corpus/corpus.db', sha256=hashlib.sha256(output.read_bytes()).hexdigest(), license='Public-Domain Scripture; MIT compilation', role='generated offline by tools/corpus/build_fixture.py'))
if output == root / 'resources/corpus/corpus.db':
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n')
print(f'{len(data["verses"])} verses → {output}')
