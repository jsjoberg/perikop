#!/usr/bin/env -S uv run --locked
"""Build resources/lexicon/study.db for the Ordstudium panel from pinned inputs.

Swedish 1917 word forms resolve to Dalin (1850) entries here, offline, so the
reader only looks forms up. Greek New Testament words keep their Strong's tags
from the Byzantine text, and TBESG supplies the Strong's entries they use.
"""
import hashlib, html, itertools, json, re, sqlite3, unicodedata, zipfile
import xml.etree.ElementTree as ET
from pathlib import Path
from fetch import fetch
root=Path(__file__).resolve().parents[2]
output=root/'resources/lexicon/study.db'

# --- Dalin entries and forms --------------------------------------------------
entries={}  # lemgram -> (headword, gram, definition)
for _,el in ET.iterparse(fetch('dalin')):
    if el.tag!='LexicalEntry':continue
    form=el.find('Lemma/FormRepresentation')
    lemgram=form.find('feat[@att="lemgram"]');word=form.find('feat[@att="writtenForm"]')
    if lemgram is not None and word is not None:
        gram=form.find('feat[@att="gram"]')
        senses=[' '.join(t.get('val') for t in sense.findall('Definition/feat[@att="text"]')) for sense in el.findall('Sense')]
        senses=[s for s in senses if s]
        if not senses:senses=[u.get('val') for u in form.findall('feat[@att="usg"]')]
        if senses:
            text=senses[0] if len(senses)==1 else '\n'.join(f'{i}. {s}' for i,s in enumerate(senses,1))
            entries[lemgram.get('val')]=(word.get('val'),gram.get('val') if gram is not None else '',text)
    el.clear()
forms={}  # lowercase Dalin-spelling form -> lemgrams
for lemgram,(word,_,_) in entries.items():forms.setdefault(word.lower(),set()).add(lemgram)
for _,el in ET.iterparse(fetch('dalinm')):
    if el.tag!='LexicalEntry':continue
    lemgram=el.find('Lemma/FormRepresentation/feat[@att="lemgram"]').get('val')
    if lemgram in entries:
        for wordform in el.findall('WordForm/feat[@att="writtenForm"]'):
            if not wordform.get('val').endswith('-'):forms.setdefault(wordform.get('val').lower(),set()).add(lemgram)
    el.clear()

# --- Resolve Swedish 1917 forms -----------------------------------------------
def spellings(word):
    """The 1906 reform wrote v for f/fv, v for hv, kv for qv, ä for e, tt for dt."""
    options=[]
    for i,c in enumerate(word):
        if c=='v' and i>0:options.append(('v','f','fv'))
        elif c=='ä':options.append(('ä','e'))
        else:options.append((c,))
    result=[]
    for letters in itertools.islice(itertools.product(*options),64):
        candidate=''.join(letters)
        for variant in (candidate,'h'+candidate if candidate.startswith('v') else None,candidate.replace('kv','qv'),
                        candidate[:-2]+'dt' if candidate.endswith('tt') else None):
            if variant and variant not in result:result.append(variant)
    return result
def verb(lemgram):
    # Neither the lemgram code nor the grammar field ("v. a. 2.") marks every verb.
    gram=entries[lemgram][1]
    return '..vb.' in lemgram or gram.startswith('v.') or bool(re.match(r'(Ind|Indik|Impf|Imp|Pres)\b',gram))
def found(word,verbs=False):
    for candidate in spellings(word):
        lemgrams={l for l in forms.get(candidate,()) if not verbs or verb(l)}
        if lemgrams:return lemgrams
    return set()
irregular={}
for line in (root/'resources/lexicon/sv1917-forms.tsv').read_text(encoding='utf-8').splitlines():
    if line and not line.startswith('#'):
        form,headword,*part=line.split('\t');irregular[form]=(headword,part==['vb'])
# Verb endings Dalin's morphology lacks: preterite, supine, present, 2nd plural, -o plural.
VERB_ENDINGS=[('ade','a'),('ades','as'),('at','a'),('ar','a'),('er','a'),('en','a'),('dde','a'),('de','a'),('te','a'),('tt',''),('t','a'),('o','a'),('r','')]
NOUN_ENDINGS=['erna','arna','orna','na','ens','ets','en','et','s']
def resolve(word):
    if word in irregular:
        headword,verbs=irregular[word]
        lemgrams={l for l in forms.get(headword,()) if entries[l][0].lower()==headword}
        if not lemgrams:raise SystemExit(f'sv1917-forms.tsv: no Dalin entry for {headword}')
        return {l for l in lemgrams if verb(l)} if verbs and any(verb(l) for l in lemgrams) else lemgrams
    if lemgrams:=found(word):return lemgrams
    for ending,replacement in VERB_ENDINGS:
        if word.endswith(ending) and len(word)-len(ending)>=2 and (lemgrams:=found(word[:-len(ending)]+replacement,verbs=True)):return lemgrams
    for ending in NOUN_ENDINGS:
        if word.endswith(ending) and len(word)-len(ending)>=3 and (lemgrams:=found(word[:-len(ending)])):return lemgrams
    return set()
corpus=sqlite3.connect(f"file:{root/'resources/corpus/corpus.db'}?mode=ro",uri=True)
counts={}
for (text,) in corpus.execute("SELECT text FROM verse JOIN source ON source.id=source_id WHERE source.code='sv1917'"):
    for word in re.findall(r"[^\W\d_]+",text):counts[word.lower()]=counts.get(word.lower(),0)+1
resolved={word:resolve(word) for word in counts}

# --- Strong's entries and Greek New Testament words ----------------------------
def plain(text):
    text=re.sub(r'<br\s*/?>','\n',text,flags=re.I)
    text=re.sub(r'<[^>]+>','',text).replace('__','')
    lines=[re.sub(r'\s+',' ',html.unescape(line)).strip() for line in text.split('\n')]
    return '\n'.join(line for line in lines if line)
strongs={}
for line in fetch('tbesg').read_text(encoding='utf-8').splitlines():
    fields=line.split('\t')
    if len(fields)>=8 and re.fullmatch(r'G\d{4}[A-Za-z]?',fields[0]) and fields[0] not in strongs:
        # NFC, as the Scripture text: TBESG writes oxia (U+1F71) where the text has tonos (U+03AC).
        strongs[fields[0]]=tuple(unicodedata.normalize('NFC',f) for f in (fields[3].strip(),fields[4].strip(),fields[6].strip(),plain(fields[7])))
NEW_TESTAMENT={'MAT':'Matt','MRK':'Mark','LUK':'Luke','JHN':'John','ACT':'Acts','ROM':'Rom','1CO':'1Cor','2CO':'2Cor','GAL':'Gal',
    'EPH':'Eph','PHP':'Phil','COL':'Col','1TH':'1Thess','2TH':'2Thess','1TI':'1Tim','2TI':'2Tim','TIT':'Titus','PHM':'Philemon',
    'HEB':'Heb','JAS':'James','1PE':'1Peter','2PE':'2Peter','1JN':'1John','2JN':'2John','3JN':'3John','JUD':'Jude','REV':'Rev'}
words=[]
with zipfile.ZipFile(root/'resources/corpus/input/grcbyz_usfm.zip') as archive:
    for name in sorted(archive.namelist()):
        text=archive.read(name).decode('utf-8-sig');usfm=re.search(r'\\id (\w+)',text)
        if not usfm or usfm[1] not in NEW_TESTAMENT:continue
        text=re.sub(r'\\(?:f|x|fe)\s.*?\\(?:f|x|fe)\*','',text,flags=re.S)
        chunks=re.split(r'\\c\s+(\d+)',text)
        for i in range(1,len(chunks),2):
            parts=re.split(r'\\v\s+(\d+)([a-z]?)(?:-\d+)?\s+',chunks[i+1])
            for k in range(1,len(parts),3):
                tags=re.findall(r'\\\+?w\s+([^|\\]+)\|[^\\]*?strong="(G\d+[A-Za-z]?)"[^\\]*\\\+?w\*',parts[k+2])
                for position,(surface,strong) in enumerate(tags):
                    words.append((NEW_TESTAMENT[usfm[1]],int(chunks[i]),int(parts[k]),parts[k+1],position,unicodedata.normalize('NFC',surface.strip()),strong))
def strong_key(strong):
    # Tags use four digits, as G0746; TBESG may add a disambiguating letter.
    return strong if strong in strongs else next((k for k in (strong[:5]+c for c in 'ABCDEFGH') if k in strongs),None)
used={strong_key(w[6]) for w in words}-{None}

# --- Write ---------------------------------------------------------------------
output.parent.mkdir(parents=True,exist_ok=True)
temp=output.with_suffix('.tmp');temp.unlink(missing_ok=True)
db=sqlite3.connect(temp)
db.executescript('''
PRAGMA user_version=1; PRAGMA application_id=1330795348;
CREATE TABLE dalin(id INTEGER PRIMARY KEY, headword TEXT NOT NULL, gram TEXT NOT NULL, definition TEXT NOT NULL) STRICT;
CREATE TABLE sv_word(form TEXT NOT NULL, dalin INTEGER NOT NULL REFERENCES dalin(id), PRIMARY KEY(form,dalin)) STRICT, WITHOUT ROWID;
CREATE TABLE strongs(strong TEXT PRIMARY KEY, lemma TEXT NOT NULL, transliteration TEXT NOT NULL, gloss TEXT NOT NULL, definition TEXT NOT NULL) STRICT, WITHOUT ROWID;
CREATE TABLE greek_word(book TEXT NOT NULL, chapter INTEGER NOT NULL, verse INTEGER NOT NULL, suffix TEXT NOT NULL, position INTEGER NOT NULL,
 surface TEXT NOT NULL, strong TEXT NOT NULL, PRIMARY KEY(book,chapter,verse,suffix,position)) STRICT, WITHOUT ROWID;
''')
ids={lemgram:i for i,lemgram in enumerate(sorted({l for ls in resolved.values() for l in ls}),1)}
db.executemany('INSERT INTO dalin VALUES(?,?,?,?)',[(i,*entries[l]) for l,i in ids.items()])
db.executemany('INSERT INTO sv_word VALUES(?,?)',sorted((word,ids[l]) for word,ls in resolved.items() for l in ls))
db.executemany('INSERT INTO strongs VALUES(?,?,?,?,?)',sorted((k,*strongs[k]) for k in used))
db.executemany('INSERT INTO greek_word VALUES(?,?,?,?,?,?,?)',[(*w[:6],strong_key(w[6]) or w[6]) for w in words])
db.commit();db.execute('VACUUM');db.close();temp.replace(output)

total=sum(counts.values());covered=sum(n for w,n in counts.items() if resolved[w])
print(f'Swedish forms: {sum(1 for w in counts if resolved[w])}/{len(counts)} resolved, {100*covered/total:.1f}% of word occurrences')
print(f'Dalin entries: {len(ids)}; Strong\'s entries: {len(used)}; Greek words: {len(words)}, untagged in lexicon: {sum(1 for w in words if not strong_key(w[6]))}')
manifest=root/'resources/manifest.json';metadata=json.loads(manifest.read_text())
for asset in metadata['assets']:
    if asset.get('path') in ('resources/lexicon/study.db','resources/lexicon/sv1917-forms.tsv'):
        asset['sha256']=hashlib.sha256((root/asset['path']).read_bytes()).hexdigest()
manifest.write_text(json.dumps(metadata,ensure_ascii=False,indent=2)+'\n')
