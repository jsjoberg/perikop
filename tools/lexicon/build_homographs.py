#!/usr/bin/env -S uv run --locked
"""List the Swedish 1917 words that NST pronounces in more than one way.

Writes resources/lexicon/sv1917-homographs.tsv: form, occurrences, voice,
alternatives. The form is lowercase, as the voice looks words up. The voice
column is what the bundled pronunciation data says without bundled corrections.
Alternatives are "phonemes=label" pairs separated by "|", in eSpeak NG
Swedish phoneme codes; a label names the parts of speech and adds "böjd form" when
NST lists the pronunciation only as an inflection of another word, such as
"kors" from "ko". The pronunciation review offers these as choices.

Reads the pronunciation data in resources/speech.
"""
import argparse, collections, hashlib, json, re, sqlite3, tarfile
from pathlib import Path
from fetch import fetch
from phonemes import to_espeak, to_ipa
root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser()
parser.add_argument('--pack',type=Path,default=root/'resources/speech')
args=parser.parse_args()

# NST SAMPA to the bundled NST-derived IPA. The mapping reproduces every
# corpus word the pack's lexicon shares with NST.

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
        entry=variants[form].setdefault(to_espeak(ipa),[set(),False])
        entry[0].add(POS.get(fields[1].split('|')[0],fields[1].split('|')[0] or 'okänd'))
        entry[1]|='LEX' in fields[5]

# The voice's own choice: the lexicon, then the precomputed corpus pronunciations.
def table(name):
    path=args.pack/name
    return dict(line.split('\t') for line in path.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#'))
voice={**table('g2p-corpus.tsv'),**table('lexicon.tsv')}
def spoken(form):
    return voice.get(form,'').replace(' ','')

rows=[]
for form,found in variants.items():
    if len(found)<2:continue
    alternatives='|'.join(f'{phones}={", ".join(sorted(pos))}{"" if lex else ", böjd form"}'
                          for phones,(pos,lex) in sorted(found.items()))
    rows.append((form,counts[form],spoken(form),alternatives))
rows.sort(key=lambda r:(-r[1],r[0]))
out=root/'resources/lexicon/sv1917-homographs.tsv'
with out.open('w',encoding='utf-8',newline='\n') as f:
    f.write('# form\toccurrences\tvoice\talternatives\n')
    for row in rows:f.write('\t'.join(map(str,row))+'\n')
manifest=root/'resources/manifest.json'
metadata=json.loads(manifest.read_text(encoding='utf-8'))
asset={"path":"resources/lexicon/sv1917-homographs.tsv",
       "url":json.loads((Path(__file__).parent/'inputs.json').read_text(encoding='utf-8'))["nst"]["url"],
       "sha256":hashlib.sha256(out.read_bytes()).hexdigest(),"license":"CC0-1.0 (NST); public-domain corpus word counts",
       "role":"Generated Swedish 1917 words with several NST pronunciations, for the pronunciation review"}
metadata['assets']=[value for value in metadata['assets'] if value.get('path')!=asset['path']]+[asset]
manifest.write_text(json.dumps(metadata,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
print(f'{len(rows)} words with several pronunciations, {sum(r[1] for r in rows)} occurrences; '
      f'{sum(not r[2] for r in rows)} without a voice entry')
