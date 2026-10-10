#!/usr/bin/env -S uv run --locked
"""List the Swedish 1917 words that NST pronounces in more than one way.

Writes resources/lexicon/sv1917-homographs.tsv: form, occurrences, voice,
alternatives. The form is lowercase, as the voice looks words up. The voice
column is what the prepared voice pack says without bundled corrections.
Alternatives are "phonemes=label" pairs separated by "|", in the voice's
phoneme symbols; a label names the parts of speech and adds "böjd form" when
NST lists the pronunciation only as an inflection of another word, such as
"kors" from "ko". The pronunciation review offers these as choices.

Needs the voice pack from tools/speech/prepare_kokoro.py (default build/kokoro-pack).
"""
import argparse, collections, hashlib, json, re, sqlite3, tarfile
from pathlib import Path
from fetch import fetch
root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser()
parser.add_argument('--pack',type=Path,default=root/'build/kokoro-pack')
args=parser.parse_args()

# NST SAMPA to the voice pack's NST-derived IPA. The mapping reproduces every
# corpus word the pack's lexicon shares with NST.
SYMBOLS=sorted({'x\\':'ɧ',"s'":'ɕ','s`':'ʂ','n`':'ɳ','l`':'ɭ','t`':'ʈ','d`':'ɖ','u0':'ɵ','}':'ʉ','2':'ø','9':'œ',
                'E':'ɛ','O':'ɔ','U':'ʊ','I':'ɪ','Y':'ʏ','N':'ŋ','A':'ɑ','{':'æ','@':'ə',':':'ː','g':'ɡ',
                **{c:c for c in 'abdefhijklmnoprstuvwyz'}}.items(),key=lambda kv:-len(kv[0]))
VOWELS=set('ɵʉøœɛɔʊɪʏɑæəaeiouy')
def to_ipa(sampa):
    out=''
    for raw in sampa.split('$'):
        stress,phones,i='',[],0
        while i<len(raw):
            if raw[i] in '"%':
                stress='ˈ' if raw[i]=='"' else 'ˌ'
                i+=1
                continue
            for k,v in SYMBOLS:
                if raw.startswith(k,i):
                    phones.append(v);i+=len(k);break
            else:return None
        # The stress mark goes directly before the syllable's vowel; an unstressed short e is a schwa.
        text=''
        for j,p in enumerate(phones):
            long=j+1<len(phones) and phones[j+1]=='ː'
            if p in VOWELS and stress and not any(c in VOWELS for c in text):text+=stress
            text+='ə' if p=='e' and not stress and not long else p
        out+=text
    return out

POS={'NN':'substantiv','VB':'verb','JJ':'adjektiv','AB':'adverb','PN':'pronomen','PS':'pronomen','HP':'pronomen',
     'HD':'pronomen','HS':'pronomen','DT':'pronomen','HA':'adverb','PP':'preposition','KN':'konjunktion',
     'SN':'subjunktion','RG':'räkneord','RO':'räkneord','PM':'namn','IN':'interjektion','PC':'particip',
     'PF':'particip','PL':'partikel','IE':'infinitivmärke','UO':'utländskt ord'}

counts=collections.Counter()
db=sqlite3.connect(f'file:{root/"resources/corpus/corpus.db"}?mode=ro',uri=True)
for (text,) in db.execute("SELECT v.text FROM verse v JOIN source s ON s.id=v.source_id WHERE s.code='sv1917'"):
    counts.update(w.lower() for w in re.findall(r"[^\W\d_]+",text))
variants=collections.defaultdict(dict)  # form -> phonemes -> [parts of speech, headword]
with tarfile.open(fetch('nst')) as archive:
    member=next(m for m in archive.getmembers() if m.isfile() and m.name.endswith('.pron'))
    for raw in archive.extractfile(member):
        fields=raw.decode('latin-1').rstrip('\n').split(';')
        form=fields[0].lower()
        if len(fields)<=11 or form not in counts or not (ipa:=to_ipa(fields[11])):continue
        entry=variants[form].setdefault(ipa,[set(),False])
        entry[0].add(POS.get(fields[1].split('|')[0],fields[1].split('|')[0] or 'okänd'))
        entry[1]|='LEX' in fields[5]

# The voice's own choice: upstream overrides, then the lexicon, then the
# precomputed neural fallback, with KokoroText::ipa's fixed replacements.
def table(name):
    path=args.pack/name
    return dict(line.split('\t') for line in path.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#'))
voice={**table('g2p-corpus.tsv'),**table('lexicon.tsv'),**{k.lower():v for k,v in table('custom_lexicon.tsv').items()}}
def spoken(form):
    phones=voice.get(form,'').replace(' ','')
    for a,b in [('ˈuːɕˌɛj','ˈuːkɛj'),('uːəsˈɛs','ˈɔs')]:phones=phones.replace(a,b)
    return phones

rows=[]
for form,found in variants.items():
    if len(found)<2:continue
    alternatives='|'.join(f'{ipa}={", ".join(sorted(pos))}{"" if lex else ", böjd form"}'
                          for ipa,(pos,lex) in sorted(found.items()))
    rows.append((form,counts[form],spoken(form),alternatives))
rows.sort(key=lambda r:(-r[1],r[0]))
out=root/'resources/lexicon/sv1917-homographs.tsv'
with out.open('w',encoding='utf-8',newline='\n') as f:
    f.write('# form\toccurrences\tvoice\talternatives\n')
    for row in rows:f.write('\t'.join(map(str,row))+'\n')
manifest=root/'resources/manifest.json'
metadata=json.loads(manifest.read_text())
asset={"path":"resources/lexicon/sv1917-homographs.tsv",
       "url":json.loads((Path(__file__).parent/'inputs.json').read_text())["nst"]["url"],
       "sha256":hashlib.sha256(out.read_bytes()).hexdigest(),"license":"CC0-1.0 (NST); public-domain corpus word counts",
       "role":"Generated Swedish 1917 words with several NST pronunciations, for the pronunciation review"}
metadata['assets']=[value for value in metadata['assets'] if value.get('path')!=asset['path']]+[asset]
manifest.write_text(json.dumps(metadata,ensure_ascii=False,indent=2)+'\n')
print(f'{len(rows)} words with several pronunciations, {sum(r[1] for r in rows)} occurrences; '
      f'{sum(not r[2] for r in rows)} without a voice entry')
