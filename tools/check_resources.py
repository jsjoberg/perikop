#!/usr/bin/env python3
"""Check resource hashes and the bundled fonts' corpus glyph coverage offline."""
import hashlib
import json
from pathlib import Path
import struct
root=Path(__file__).resolve().parents[1]
manifest=json.loads((root/'resources/manifest.json').read_text())
checks=0
for asset in manifest['assets']:
    if not asset.get('path'): continue
    actual=hashlib.sha256((root/asset['path']).read_bytes()).hexdigest()
    if actual!=asset['sha256']: raise SystemExit(f'Hash mismatch: {asset["path"]}')
    checks+=1

def cmap(path):
    data=path.read_bytes()
    def u16(offset): return struct.unpack_from('>H',data,offset)[0]
    def u32(offset): return struct.unpack_from('>I',data,offset)[0]
    table=None
    for i in range(u16(4)):
        row=12+i*16
        if data[row:row+4]==b'cmap': table=u32(row+8)
    if table is None: raise ValueError('No cmap')
    points=set()
    for i in range(u16(table+2)):
        record=table+4+i*8
        platform=u16(record);encoding=u16(record+2)
        if platform not in (0,3) or (platform==3 and encoding not in (1,10)): continue
        offset=table+u32(record+4)
        format=u16(offset)
        if format==12:
            for j in range(u32(offset+12)):
                begin,end,glyph=struct.unpack_from('>III',data,offset+16+12*j)
                points.update(range(begin+(glyph==0),end+1))
        if format==4:
            count=u16(offset+6)//2;ends=offset+14;starts=ends+2*count+2;deltas=starts+2*count;ranges=deltas+2*count
            for j in range(count):
                for c in range(u16(starts+2*j),u16(ends+2*j)+1):
                    displacement=u16(ranges+2*j)
                    glyph=u16(ranges+2*j+displacement+2*(c-u16(starts+2*j))) if displacement else c
                    if glyph: glyph=(glyph+u16(deltas+2*j))&65535
                    if glyph:points.add(c)
    return points
import sqlite3
with sqlite3.connect('file:'+str(root/'resources/corpus/corpus.db')+'?mode=ro',uri=True) as db:
    required={ord(c) for (text,) in db.execute('SELECT text FROM verse') for c in text if not c.isspace()}
    count=db.execute('SELECT count(*) FROM verse').fetchone()[0]
    if count < 100000:raise SystemExit('The complete corpus is missing')
    bad=db.execute("SELECT count(*) FROM verse WHERE text LIKE '%strong=%' OR text LIKE '%<%' OR text LIKE '%\\%'").fetchone()[0]
    if bad:raise SystemExit(f'{bad} verses still contain import markup')
for name in ['Literata-Regular.ttf','Literata-Italic.ttf']:
    fallback=cmap(root/'resources/fonts/NotoSerifHebrew-Regular.ttf')|cmap(root/'resources/fonts/NotoSansMath-Regular.ttf')
    missing=required-(cmap(root/'resources/fonts'/name)|fallback)
    if missing:raise SystemExit(f'{name}: missing glyphs '+', '.join(f'U+{c:04X}' for c in sorted(missing)))
    print(f'{name}: all {len(required)} corpus characters covered, including bundled fallback')
print(f'{checks} resource hashes passed')
