#!/usr/bin/env -S uv run --locked
"""List every word form in the Swedish 1917 text with its NST pronunciation.

Writes resources/lexicon/sv1917-words.tsv: form, occurrences, kind, NST SAMPA.
Kind is "name" for forms that only occur capitalised, otherwise "word".
An empty SAMPA column means NST has no entry and the form needs review.
"""
import collections, re, sqlite3, tarfile, hashlib, json
from pathlib import Path
from fetch import fetch
root=Path(__file__).resolve().parents[2]
counts=collections.Counter()
db=sqlite3.connect(root/'resources/corpus/corpus.db')
for (text,) in db.execute("SELECT v.text FROM verse v JOIN source s ON s.id=v.source_id WHERE s.code='sv1917'"):
    counts.update(re.findall(r"[^\W\d_]+",text))
lowered={form.lower() for form in counts if not form[0].isupper()}
# NST: semicolon fields, Latin-1; field 0 is the form and field 11 its SAMPA.
wanted={form for form in counts}|{form.lower() for form in counts}
nst={}
with tarfile.open(fetch('nst')) as archive:
    member=next(m for m in archive.getmembers() if m.isfile() and m.name.endswith('.pron'))
    for raw in archive.extractfile(member):
        fields=raw.decode('latin-1').rstrip('\n').split(';')
        if len(fields)>11 and fields[0] in wanted and fields[0] not in nst:nst[fields[0]]=fields[11]
rows=[]
for form,count in counts.items():
    name=form[0].isupper() and form.lower() not in lowered
    sampa=nst.get(form) or nst.get(form.lower(),'')
    rows.append((form,count,'name' if name else 'word',sampa))
rows.sort(key=lambda r:(-r[1],r[0]))
out=root/'resources/lexicon/sv1917-words.tsv'
out.parent.mkdir(parents=True,exist_ok=True)
with out.open('w',encoding='utf-8') as f:
    f.write('# form\toccurrences\tkind\tnst_sampa\n')
    for row in rows:f.write('\t'.join(map(str,row))+'\n')
manifest=root/'resources/manifest.json'
metadata=json.loads(manifest.read_text())
asset={"path":"resources/lexicon/sv1917-words.tsv",
       "url":json.loads((Path(__file__).parent/'inputs.json').read_text())["nst"]["url"],
       "sha256":hashlib.sha256(out.read_bytes()).hexdigest(),"license":"CC0-1.0 (NST); public-domain corpus word counts",
       "role":"Generated Swedish 1917 word forms with NST SAMPA references"}
metadata['assets']=[value for value in metadata['assets'] if value.get('path')!=asset['path']]+[asset]
manifest.write_text(json.dumps(metadata,ensure_ascii=False,indent=2)+'\n')
missing=[r for r in rows if not r[3]]
print(f'{len(rows)} forms, {sum(r[2]=="name" for r in rows)} names; NST covers {len(rows)-len(missing)}; '
      f'missing {len(missing)} ({sum(r[2]=="name" for r in missing)} names, {sum(r[1] for r in missing)} occurrences)')
