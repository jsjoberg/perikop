#!/usr/bin/env -S uv run --locked
"""Build the full offline corpus and recurring lectionary tables from pinned inputs."""
import json, re, sqlite3, zipfile, hashlib
from pathlib import Path
import runeberg1917
root=Path(__file__).resolve().parents[2]
inputs=root/'resources/corpus/input'
# OSIS codes preserve the existing public API; USFM identifies Greek inputs.
def table(name):
 return [line.split('\t') for line in (root/'resources/corpus'/name).read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
entries=table('books.tsv')
# Keep original IDs used by the curated Psalm alignment records.
ids={'Ps':1,'Luke':2,'Phil':3}
for code,*_ in entries:
 if code not in ids:ids[code]=len(ids)+1
byname={name:code for code,_,name,*_ in entries}
byusfm={usfm:code for code,usfm,*_ in entries}
verses=[];greek_names={};paragraphs=[]
# Swedish keeps its printed 1917/1921 verse numbers; see runeberg1917.py and apocrypha1921.py.
for (code,chapter,verse),text in runeberg1917.read(inputs/'runeberg-bibeln-1917.html').items():
 verses.append((1,ids[code],chapter,verse,verse,'',text))
for line in (inputs/'sv1921-apokryfer.tsv').read_text(encoding='utf-8').splitlines():
 code,chapter,verse,text=line.split('\t');verses.append((1,ids[code],int(chapter),int(verse),int(verse),'',text))
for source,filename in [(4,'KJV.json')]:
 data=json.loads((inputs/filename).read_text())
 for book in data['books']:
  name=book['name'];code=byname.get(name)
  if not code:raise ValueError('Unknown book '+name)
  for ch in book['chapters']:
   for v in ch['verses']:
    if v['text'].strip():verses.append((source,ids[code],ch['chapter'],v['verse'],v['verse'],'',v['text']))
for source,filename in [(2,'grcbrent_usfm.zip'),(3,'grcbyz_usfm.zip'),(5,'eng-web_usfm.zip')]:
 with zipfile.ZipFile(inputs/filename) as z:
  for filename in z.namelist():
   if not filename.endswith('.usfm'):continue
   text=z.read(filename).decode('utf-8-sig');m=re.search(r'\\id (\w+)',text)
   if not m:raise ValueError('USFM book identifier missing: '+filename)
   if m[1] not in byusfm:
    if m[1] in ('FRT','INT','GLO','BAK','OTH','XXA','XXB','XXC'):continue
    raise ValueError('Unmapped Scripture book: '+m[1])
   code='Dan' if source==2 and m[1]=='DAG' else byusfm[m[1]];h=re.search(r'\\h ([^\n]+)',text)
   if h and source in (2,3):greek_names[code]=h[1].strip()
   # Notes, cross references and Strong's attributes are outside display text.
   text=re.sub(r'\\(?:f|x|fe)\s.*?\\(?:f|x|fe)\*','',text,flags=re.S)
   text=re.sub(r'\\\+?w\s+([^|\\]+)(?:\|[^\\]*)?\\\+?w\*',r'\1',text)
   # The reader's fonts have no white square brackets; [[ ]] keeps them distinct from [ ].
   if source==3:text=text.replace('⟦','[[').replace('⟧',']]')
   for label in re.findall(r'\\v\s+(\S+)',text):
    if not re.fullmatch(r'\d+[a-z]?(?:-\d+)?',label):raise ValueError('Unsupported verse label: '+filename+' '+label)
   chapter=0
   chunks=re.split(r'\\c\s+(\d+)',text)
   for i in range(1,len(chunks),2):
    chapter=int(chunks[i]);body=chunks[i+1]
    vs=list(re.finditer(r'\\v\s+(\d+[a-z]?(?:-\d+)?)\s+',body))
    poetry=False
    for k,v in enumerate(vs):
     prefix=body[:v.start()] if k==0 else body[vs[k-1].end():v.start()]
     markers=list(re.finditer(r'\\(p|m|pi\d*|q\d*|b)\b',prefix))
     if markers:poetry=markers[-1][1].startswith('q')
     # A prose break inside a verse must not migrate to the next verse.
     boundary=poetry or bool(markers and not prefix[markers[-1].end():].strip())
     chunk=body[v.end():vs[k+1].start() if k+1<len(vs) else len(body)]
     # Non-Scripture section headings belong outside the preceding verse.
     chunk=re.sub(r'\\(?:s\d*|ms\d*|r|d|sp|cl|cp)\s+[^\n]*','',chunk)
     chunk=re.sub(r'\\[+a-zA-Z0-9]+\*?\s*',' ',chunk)
     plain=' '.join(chunk.split())
     if not plain:continue
     label=re.fullmatch(r'(\d+)([a-z]?)(?:-(\d+))?',v[1]);first=int(label[1]);last=int(label[3] or label[1])
     verses.append((source,ids[code],chapter,first,last,label[2],plain))
     if k==0 or boundary:paragraphs.append((source,ids[code],chapter,first,label[2],'USFM'))
output=root/'resources/corpus/corpus.db';temp=output.with_suffix('.tmp');temp.unlink(missing_ok=True)
with sqlite3.connect(temp) as db:
 db.execute('PRAGMA page_size=4096')
 db.execute('PRAGMA journal_mode=DELETE')
 db.execute('PRAGMA synchronous=FULL')
 db.executescript((root/'resources/corpus/schema.sql').read_text())
 sources=[(1,'sv1917','sv','Svenska 1917 med apokryfer','SV1917','Public-Domain','Swedish 1917 and 1921 apocrypha; Project Runeberg e-text and facsimile'),(2,'grc-lxx','el','Brenton Septuaginta 1851','LXX','Public-Domain','eBible.org Greek Brenton text'),(3,'grc-patriarchal','el','Patriarkal grekiska 1904','NT','Public-Domain','eBible.org 1904 Patriarchal text with corrections'),(4,'en-kjv','en','King James Version','MT','Public-Domain','Scrollmapper; UK Crown rights may apply'),(5,'en-web','en','World English Bible Classic med deuterokanon','MT','Public-Domain','eBible.org; stable 2020 text, includes deuterocanonical books')]
 db.executemany('INSERT INTO source VALUES(?,?,?,?,?,?,?)',sources)
 db.executemany('INSERT INTO book VALUES(?,?,?,?,?,?,?,?,?,?)',[(ids[c],c,i+1,sv,greek_names.get(c,''),name,abbreviation,int(testament=='NT'),int(deutero),int(stanzas)) for i,(c,_,name,sv,abbreviation,testament,deutero,stanzas) in enumerate(entries)])
 for position,(book,frame,chapters) in enumerate(table('canon.tsv')):
  first,_,last=chapters.partition('-')
  db.execute('INSERT INTO canon VALUES(?,?,?,?,?)',(position,book,frame,int(first or 1),int(last) if last else None))
 db.executemany('INSERT INTO verse(source_id,book_id,chapter,verse,last_verse,verse_suffix,text) VALUES(?,?,?,?,?,?,?)',verses)
 # Versification alignment generated by align_editions.py from reviewed rules.
 code_id={code:i for i,code,*_ in sources}
 def coordinate(text):
  m=re.fullmatch(r'(\d+):(\d+)([a-z]?)',text);return (int(m[1]),int(m[2]),m[3]) if m else (None,None,None)
 for line in (root/'resources/corpus/alignment.tsv').read_text().splitlines():
  a,b,book,f0,f1,tbook,t0,t1,kind=line.split('\t')
  db.execute('INSERT INTO alignment VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)',(code_id[a],code_id[b],ids[book],*coordinate(f0),*coordinate(f1),ids.get(tbook),*coordinate(t0),*coordinate(t1),int(kind)))
 db.executemany('INSERT OR IGNORE INTO paragraph VALUES(?,?,?,?,?,?)',paragraphs)
 # These are display boundaries, not claimed original Swedish/KJV punctuation.
 # Psalms have title/number differences, so retain their separate verse stanzas.
 # Swedish keeps its own numbering, so it takes only boundaries the alignment marks as the same verse.
 for source in (1,4):
  db.execute('INSERT OR IGNORE INTO paragraph SELECT ?,p.book_id,p.chapter,p.verse,p.verse_suffix,? FROM paragraph p JOIN verse v ON v.source_id=? AND v.book_id=p.book_id AND v.chapter=p.chapter AND v.verse=p.verse AND v.verse_suffix=p.verse_suffix WHERE p.source_id=5 AND p.book_id!=? AND (NOT EXISTS(SELECT 1 FROM alignment a WHERE a.from_source=5 AND a.to_source=? AND a.book_id=p.book_id) OR EXISTS(SELECT 1 FROM alignment a WHERE a.from_source=5 AND a.to_source=? AND a.book_id=p.book_id AND a.kind=0 AND p.chapter*1000+p.verse BETWEEN a.from_first_chapter*1000+a.from_first_verse AND a.from_last_chapter*1000+a.from_last_verse))',(source,'WEB editorial',source,ids['Ps'],source,source))
 db.executemany('INSERT INTO pronunciation VALUES(?,?,?,?,?)',[('sv','Melkisedek','Melki-sedek','',100)])
 # Swedish name phonemes for the Kokoro voices, plus each name's genitive -s.
 phones=set('abdefhijklmnoprstuvyøŋœɑɔɕɖəɛɡɧɪɭɳɵʂʈʉʊʏˈˌː')
 for line in (root/'resources/corpus/pronunciation-sv.tsv').read_text().splitlines():
  if not line or line.startswith('#'):continue
  word,ipa=line.split('\t')
  if not set(ipa)<=phones or ipa.count('ˈ')!=1:raise SystemExit('Invalid phonemes for '+word+': '+ipa)
  # A phrase gives its bracketed word one sense. It outranks review corrections (1000) and previews (2000).
  if '[' in word:
   m=re.fullmatch(r'([^\[\]]*)\[([^\[\]\s]+)\]([^\[\]]*)',word)
   if not m or not (m[1]+m[3]).strip():raise SystemExit('A phrase needs one [word] and context: '+word)
   db.execute('INSERT INTO pronunciation VALUES(?,?,?,?,?)',('sv',m[1]+m[2]+m[3],m[1]+'⟦'+ipa+'⟧'+m[3],'',3000))
   continue
  db.executemany('INSERT INTO pronunciation VALUES(?,?,?,?,?)',[('sv',word,'',ipa,50),('sv',word+'s','',ipa+'s',50)])
 # Import recurring references, never the project's third-party Scripture wording.
 # Rows are tagged common, greek or slavic; antiochian.json adds the Antiochian
 # Archdiocese's own readings in the same format, each in the slot it replaces.
 tables=[x for name in ('orthocal-tables.json','antiochian.json') for x in json.loads((root/'resources/lectionary'/name).read_text())]
 # Orthocal follows KJV numbering, except where a reference only exists in the Septuagint.
 new_testament={code for code,_,_,_,_,testament,*_ in entries if testament=='NT'}
 kjv_last={(b,c):v for b,c,v in db.execute('SELECT book.code,chapter,max(verse) FROM verse JOIN book ON book.id=book_id WHERE source_id=4 GROUP BY book_id,chapter')}
 pericopes={x['pk']:x['fields'] for x in tables if x['model']=='calendarium.pericope'}
 for x in tables:
  f=x['fields'];model=x['model']
  if model=='calendarium.day':
   db.execute('INSERT INTO feast_rule VALUES(?,?,?,?,?,?,?,?,?,?,?)',(x['pk'],f['pdist'],f['month'],f['day'],f['feast_level'],f['title'],f['feast_name'],f['tradition'],f.get('fast'),f.get('fast_exception'),f.get('fast_cap_exempt')))
  elif model=='calendarium.ordoreading':
   db.execute('INSERT INTO ordo_rule VALUES(?,?,?,?,?,?)',(f['jurisdiction'],f['year'],f['month'],f['day'],f['source'],f['pdist']))
  elif model=='calendarium.reading':
   p=pericopes[f['pericope']];parts=p['verses'].strip().split('|')
   composite=parts[0].startswith('Comp_')
   septuagint=not composite and ('LXX' in p['display'] or any(b not in new_testament and kjv_last.get((b,int(last)//1000),0)<int(last)%1000 for b,_,last in (part.strip().replace('3Kg','1Kgs').replace('4Kg','2Kgs').split('_') for part in parts)))
   db.execute('INSERT INTO reading_rule VALUES(?,?,?,?,?,?,?,?,?,?)',(x['pk'],f['pdist'],f['month'],f['day'],f['source'],f['desc'],f['ordering'],f['tradition'],p['display'],2 if septuagint else 4))
   if composite:continue # retain the citation; never substitute a guessed Scripture range
   for idx,part in enumerate(parts):
    book,first,last=part.strip().split('_');book={'3Kg':'1Kgs','4Kg':'2Kgs'}.get(book,book)
    first,last=int(first),int(last)
    db.execute('INSERT INTO reading_segment VALUES(?,?,?,?,?,?,?)',(x['pk'],idx,book,first//1000,first%1000,last//1000,last%1000))
 for f in json.loads((root/'resources/lectionary/commemorations.json').read_text()):
  db.execute('INSERT INTO commemoration_rule VALUES(?,?,?,?,?,?,?)',(f['pk'],f['day'],f['ordering'],f['title'],f['tradition'],int(f['new_style']),int(f['day_native'])))
 db.execute('PRAGMA user_version=6')
 db.execute('PRAGMA application_id=1330795587')
 db.execute('ANALYZE')
 assert not db.execute('PRAGMA foreign_key_check').fetchall()
 assert db.execute('PRAGMA integrity_check').fetchone()[0]=='ok'
 print('Text:',db.execute('SELECT code,count(*),count(distinct book_id) FROM verse JOIN source ON source.id=source_id GROUP BY source_id').fetchall())
 print('Calendar:',db.execute('SELECT count(*) FROM reading_rule').fetchone()[0],'recurring pericopes')
 db.commit()
temp.replace(output)

manifest=root/'resources/manifest.json'
# Generated resources; the other inputs are pinned downloads.
GENERATED={'resources/corpus/corpus.db','resources/corpus/schema.sql','resources/corpus/books.tsv','resources/corpus/canon.tsv','resources/lectionary/antiochian.json','resources/corpus/alignment.tsv','resources/corpus/input/sv1921-apokryfer.tsv','resources/corpus/pronunciation-sv.tsv'}
metadata=json.loads(manifest.read_text())
metadata['schema_version']=3
for asset in metadata['assets']:
 if asset.get('path') in GENERATED:asset['sha256']=hashlib.sha256((root/asset['path']).read_bytes()).hexdigest()
manifest.write_text(json.dumps(metadata,ensure_ascii=False,indent=2)+'\n')
