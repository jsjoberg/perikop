#!/usr/bin/env python3
"""Build the full offline corpus and recurring lectionary tables from pinned inputs."""
import json, re, sqlite3, zipfile, hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[2]
inputs=root/'resources/corpus/input'
# OSIS codes preserve the existing public API; USFM identifies Greek inputs.
entries=[
('Gen','GEN','Genesis','Första Moseboken'),('Exod','EXO','Exodus','Andra Moseboken'),('Lev','LEV','Leviticus','Tredje Moseboken'),('Num','NUM','Numbers','Fjärde Moseboken'),('Deut','DEU','Deuteronomy','Femte Moseboken'),('Josh','JOS','Joshua','Josua'),('Judg','JDG','Judges','Domarboken'),('Ruth','RUT','Ruth','Rut'),('1Sam','1SA','I Samuel','Första Samuelsboken'),('2Sam','2SA','II Samuel','Andra Samuelsboken'),('1Kgs','1KI','I Kings','Första Kungaboken'),('2Kgs','2KI','II Kings','Andra Kungaboken'),('1Chr','1CH','I Chronicles','Första Krönikeboken'),('2Chr','2CH','II Chronicles','Andra Krönikeboken'),('Ezra','EZR','Ezra','Esra'),('Neh','NEH','Nehemiah','Nehemja'),('Esth','EST','Esther','Ester'),('Job','JOB','Job','Job'),('Ps','PSA','Psalms','Psaltaren'),('Prov','PRO','Proverbs','Ordspråksboken'),('Eccl','ECC','Ecclesiastes','Predikaren'),('Song','SNG','Song of Solomon','Höga Visan'),('Isa','ISA','Isaiah','Jesaja'),('Jer','JER','Jeremiah','Jeremia'),('Lam','LAM','Lamentations','Klagovisorna'),('Ezek','EZK','Ezekiel','Hesekiel'),('Dan','DAN','Daniel','Daniel'),('Hos','HOS','Hosea','Hosea'),('Joel','JOL','Joel','Joel'),('Amos','AMO','Amos','Amos'),('Obad','OBA','Obadiah','Obadja'),('Jonah','JON','Jonah','Jona'),('Micah','MIC','Micah','Mika'),('Nah','NAM','Nahum','Nahum'),('Hab','HAB','Habakkuk','Habackuk'),('Zeph','ZEP','Zephaniah','Sefanja'),('Hag','HAG','Haggai','Haggai'),('Zech','ZEC','Zechariah','Sakarja'),('Mal','MAL','Malachi','Malaki'),
('Tob','TOB','Tobit','Tobit'),('Jdt','JDT','Judith','Judit'),('EsthGr','ESG','Esther (Greek)','Tillägg till Ester'),('Wis','WIS','Wisdom','Salomos vishet'),('Sir','SIR','Sirach','Jesus Syraks vishet'),('Baruch','BAR','Baruch','Baruk'),('PrAzar','S3Y','Prayer of Azariah','Asarjas bön'),('Sus','SUS','Susanna','Susanna'),('Bel','BEL','Bel and the Dragon','Bel och draken'),('1Macc','1MA','I Maccabees','Första Mackabeerboken'),('2Macc','2MA','II Maccabees','Andra Mackabeerboken'),('1Esd','1ES','I Esdras','Första Esdrasboken'),('PrMan','MAN','Prayer of Manasses','Manasses bön'),('Ps151','PS2','Additional Psalm','Psalm 151'),('3Macc','3MA','III Maccabees','Tredje Mackabeerboken'),('2Esd','2ES','II Esdras','Andra Esdrasboken'),('4Macc','4MA','IV Maccabees','Fjärde Mackabeerboken'),
('Matt','MAT','Matthew','Matteusevangeliet'),('Mark','MRK','Mark','Markusevangeliet'),('Luke','LUK','Luke','Lukasevangeliet'),('John','JHN','John','Johannesevangeliet'),('Acts','ACT','Acts','Apostlagärningarna'),('Rom','ROM','Romans','Romarbrevet'),('1Cor','1CO','I Corinthians','Första Korintierbrevet'),('2Cor','2CO','II Corinthians','Andra Korintierbrevet'),('Gal','GAL','Galatians','Galaterbrevet'),('Eph','EPH','Ephesians','Efesierbrevet'),('Phil','PHP','Philippians','Filipperbrevet'),('Col','COL','Colossians','Kolosserbrevet'),('1Thess','1TH','I Thessalonians','Första Thessalonikerbrevet'),('2Thess','2TH','II Thessalonians','Andra Thessalonikerbrevet'),('1Tim','1TI','I Timothy','Första Timotheosbrevet'),('2Tim','2TI','II Timothy','Andra Timotheosbrevet'),('Titus','TIT','Titus','Titusbrevet'),('Philemon','PHM','Philemon','Filemonbrevet'),('Heb','HEB','Hebrews','Hebreerbrevet'),('James','JAS','James','Jakobsbrevet'),('1Peter','1PE','I Peter','Första Petrusbrevet'),('2Peter','2PE','II Peter','Andra Petrusbrevet'),('1John','1JN','I John','Första Johannesbrevet'),('2John','2JN','II John','Andra Johannesbrevet'),('3John','3JN','III John','Tredje Johannesbrevet'),('Jude','JUD','Jude','Judasbrevet'),('Rev','REV','Revelation of John','Uppenbarelseboken'),('EpJer','LJE','Epistle of Jeremiah','Jeremias brev')]
# Keep original IDs used by the curated Psalm alignment records.
ids={'Ps':1,'Luke':2,'Phil':3}
for code,_,_,_ in entries:
 if code not in ids:ids[code]=len(ids)+1
byname={name:code for code,_,name,_ in entries}
byusfm={usfm:code for code,usfm,_,_ in entries}
verses=[];greek_names={}
for source,filename in [(1,'Swe1917.json'),(4,'KJV.json')]:
 data=json.loads((inputs/filename).read_text())
 for book in data['books']:
  name=book['name'];code=byname.get(name)
  if not code:raise ValueError('Unknown book '+name)
  for ch in book['chapters']:
   for v in ch['verses']:
    if v['text'].strip():verses.append((source,ids[code],ch['chapter'],v['verse'],v['text']))
for source,filename in [(2,'grcbrent_usfm.zip'),(3,'grcbyz_usfm.zip')]:
 with zipfile.ZipFile(inputs/filename) as z:
  for filename in z.namelist():
   if not filename.endswith('.usfm'):continue
   text=z.read(filename).decode('utf-8-sig');m=re.search(r'\\id (\w+)',text)
   if not m or m[1] not in byusfm:continue
   code=byusfm[m[1]];h=re.search(r'\\h ([^\n]+)',text)
   if h:greek_names[code]=h[1].strip()
   # Notes, cross references and Strong's attributes are outside display text.
   text=re.sub(r'\\(?:f|x|fe)\s.*?\\(?:f|x|fe)\*','',text,flags=re.S)
   text=re.sub(r'\\\+?w\s+([^|\\]+)(?:\|[^\\]*)?\\\+?w\*',r'\1',text)
   chapter=0
   chunks=re.split(r'\\c\s+(\d+)',text)
   for i in range(1,len(chunks),2):
    chapter=int(chunks[i]);body=chunks[i+1]
    vs=list(re.finditer(r'\\v\s+(\d+(?:-\d+)?)\s+',body))
    for k,v in enumerate(vs):
     chunk=body[v.end():vs[k+1].start() if k+1<len(vs) else len(body)]
     # Non-Scripture section headings belong outside the preceding verse.
     chunk=re.sub(r'\\(?:s\d*|ms\d*|r|d|sp|cl|cp)\s+[^\n]*','',chunk)
     chunk=re.sub(r'\\[+a-zA-Z0-9]+\*?\s*',' ',chunk)
     plain=' '.join(chunk.split())
     if not plain:continue
     nums=v[1].split('-');first=int(nums[0]);last=int(nums[-1])
     if last!=first:raise ValueError('Joined USFM verse requires explicit range: '+filename+v[1])
     verses.append((source,ids[code],chapter,first,plain))
output=root/'resources/corpus/corpus.db';temp=output.with_suffix('.tmp');temp.unlink(missing_ok=True)
with sqlite3.connect(temp) as db:
 db.executescript((root/'resources/corpus/schema.sql').read_text())
 sources=[(1,'sv1917','sv','Svenska 1917 med apokryfer','MT','Public-Domain','Swedish 1917/1921 edition; Scrollmapper transcription'),(2,'grc-lxx','el','Brenton Septuaginta 1851','LXX','Public-Domain','eBible.org Greek Brenton text'),(3,'grc-patriarchal','el','Patriarkal grekiska 1904','NT','Public-Domain','eBible.org 1904 Patriarchal text with corrections'),(4,'en-kjv','en','King James Version','MT','Public-Domain','Scrollmapper; UK Crown rights may apply')]
 db.executemany('INSERT INTO source VALUES(?,?,?,?,?,?,?)',sources)
 db.executemany('INSERT INTO book VALUES(?,?,?,?,?,?)',[(ids[c],c,i+1,sv,greek_names.get(c,''),name) for i,(c,_,name,sv) in enumerate(entries)])
 db.executemany('INSERT INTO verse(source_id,book_id,chapter,verse,text) VALUES(?,?,?,?,?)',verses)
 def mapping(src,dst,fc,fv,flv,tc,tv,tlv,kind=1):
  db.execute('INSERT INTO alignment VALUES(?,?,1,?,?,?,?,?,?,?,?,?)',(src,dst,fc,fv,fc,flv,tc,tv,tc,tlv,kind))
 for mt,lxx,length in [(23,22,6),(24,23,10),(25,24,22)]:
  mapping(1,2,mt,1,length,lxx,1,length);mapping(4,2,mt,1,length,lxx,1,length);mapping(1,4,mt,1,length,mt,1,length,0)
 mapping(1,2,22,1,30,21,1,30);mapping(1,2,22,31,31,21,31,32,2)
 mapping(1,4,22,1,1,22,1,1,3);mapping(1,4,22,2,30,22,1,29);mapping(1,4,22,31,31,22,30,31,2)
 mapping(4,2,22,1,29,21,2,30);mapping(4,2,22,30,31,21,31,32)
 db.executemany('INSERT INTO pronunciation VALUES(?,?,?,?,?)',[('sv','Melkisedek','Melki-sedek','',100),('sv','Lukasevangeliet','Lukas evangelium','',100),('sv','Filipperbrevet','Filipper brevet','',100)])
 # Import recurring references, never the project's third-party Scripture wording.
 tables=json.loads((root/'resources/lectionary/orthocal-tables.json').read_text())
 pericopes={x['pk']:x['fields'] for x in tables if x['model']=='calendarium.pericope'}
 for x in tables:
  f=x['fields'];model=x['model']
  if model=='calendarium.day':
   db.execute('INSERT INTO feast_rule VALUES(?,?,?,?,?,?,?,?)',(x['pk'],f['pdist'],f['month'],f['day'],f['feast_level'],f['title'],f['feast_name'],f['tradition']))
  elif model=='calendarium.ordoreading':
   db.execute('INSERT INTO ordo_rule VALUES(?,?,?,?,?)',(f['year'],f['month'],f['day'],f['source'],f['pdist']))
  elif model=='calendarium.reading':
   p=pericopes[f['pericope']];parts=p['verses'].strip().split('|')
   if parts[0].startswith('Comp_'):continue # liturgical composites are not contiguous Scripture
   db.execute('INSERT INTO reading_rule VALUES(?,?,?,?,?,?,?,?,?)',(x['pk'],f['pdist'],f['month'],f['day'],f['source'],f['desc'],f['ordering'],f['tradition'],p['display']))
   for idx,part in enumerate(parts):
    book,first,last=part.strip().split('_');book={'3Kg':'1Kgs','4Kg':'2Kgs'}.get(book,book)
    first,last=int(first),int(last)
    db.execute('INSERT INTO reading_segment VALUES(?,?,?,?,?,?,?)',(x['pk'],idx,book,first//1000,first%1000,last//1000,last%1000))
 # Antiochian recurring variants checked against its official 2026 chart.
 def replace_segments(rule_id,segments):
  db.execute('DELETE FROM reading_segment WHERE rule_id=?',(rule_id,))
  for i,(book,fc,fv,lc,lv) in enumerate(segments):
   db.execute('INSERT INTO reading_segment VALUES(?,?,?,?,?,?,?)',(rule_id,i,book,fc,fv,lc,lv))
 replace_segments(456,[('Acts',11,19,11,30)])
 replace_segments(194,[('Heb',11,24,11,26),('Heb',11,32,11,40)])
 # Luke the Evangelist's Epistle is longer in the Antiochian book.
 for rule in db.execute("SELECT id FROM reading_rule WHERE month=10 AND day=18 AND service='Epistle'").fetchall():
  replace_segments(rule[0],[('Col',4,5,4,11),('Col',4,14,4,18)])
 for rule in db.execute("SELECT id FROM reading_rule WHERE pdist=224 AND month=0 AND service='Epistle' AND description=''").fetchall():
  replace_segments(rule[0],[('Eph',4,1,4,7)])
 # Second Sunday after Pentecost is All Saints of Antioch.
 db.execute("INSERT INTO reading_rule VALUES(20001,63,0,0,'Epistle','All Saints of Antioch',800,'greek','Acts 11:19-30')")
 replace_segments(20001,[('Acts',11,19,11,30)])
 db.execute("INSERT INTO feast_rule VALUES(20001,63,0,0,4,'','All Saints of Antioch','greek')")
 db.execute('PRAGMA user_version=1')
 assert db.execute('PRAGMA integrity_check').fetchone()[0]=='ok'
 print('Text:',db.execute('SELECT code,count(*),count(distinct book_id) FROM verse JOIN source ON source.id=source_id GROUP BY source_id').fetchall())
 print('Calendar:',db.execute('SELECT count(*) FROM reading_rule').fetchone()[0],'recurring pericopes')
 db.commit()
temp.replace(output)

manifest=root/'resources/manifest.json'
metadata=json.loads(manifest.read_text())
for asset in metadata['assets']:
 if asset.get('path')=='resources/corpus/corpus.db':asset['sha256']=hashlib.sha256(output.read_bytes()).hexdigest()
manifest.write_text(json.dumps(metadata,ensure_ascii=False,indent=2)+'\n')
