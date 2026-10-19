#!/usr/bin/env python3
"""Freeze 50 exact Swedish 1917 passages from Perikop's licensed corpus (MIT)."""
import hashlib
import json
from pathlib import Path
import sqlite3

ROOT = Path(__file__).resolve().parents[3]
# Chapter groups never cross development/heldout. Do not move passages after scoring.
SELECTION = {
    'dev': [
        ('Gen',1,1,1),('Gen',1,2,2),('Gen',1,3,5),('Gen',3,9,10),
        ('Gen',12,1,3),('Gen',22,1,2),('Exod',3,4,6),('Exod',20,12,13),
        ('Num',6,24,26),('Deut',6,4,5),('Ruth',1,16,17),('1Sam',3,9,10),
        ('2Sam',22,2,3),('1Kgs',19,11,12),('Job',19,25,27),
        ('Ps',23,1,2),('Ps',23,3,4),('Ps',23,5,6),('Ps',46,2,3),('Ps',139,13,14),
        ('Prov',3,5,6),('Eccl',3,1,3),('Song',2,11,12),('Isa',9,6,6),
        ('Isa',40,28,29),('Jer',1,6,8),('Ezek',37,3,5),('Dan',3,16,18),
        ('Jonah',2,2,3),('Micah',6,8,8),('Matt',5,3,6),('Matt',6,9,13),
        ('Mark',4,39,40),('Luke',2,1,4),('Luke',2,8,11),('John',1,1,3),
        ('John',3,16,17),('Acts',2,8,11),('Rom',8,38,39),('1Cor',13,4,7)],
    'heldout': [('Gen',11,1,3),('Exod',14,21,22),('Ps',121,1,4),('Isa',55,10,11),
                ('Matt',13,3,6),('Luke',15,11,14),('Acts',17,22,25),
                ('Eph',6,10,13),('James',3,3,5),('Rev',21,3,5)]}
# Human-curated orthographic word categories; no synthesizer pronunciation model.
LEXICAL = {
    'names': 'abraham abrahams isak jakob jakobs mose moses aron davids david israel israels jerusalem betlehem josef maria jesus kristus herre herren herrens sedrak mesak abed nego elia samuel daniel rut noomi judéen galileen nasaret augustus kvirinius pontus asien frygien pamfylien egypten rom frygier parter meder elamiter mesopotamien',
    'archaic': 'ock intet skola skall eder edert edra huru varda varde voro äro hava hade mig dig sig icke allenast evinnerligen vederkvicker trösta ty eder ären haven giva giver bliven bliver förrän såsom vart därpå hären'}
# No automatic respelling of archaic forms or proper names in primary WER.


def main():
    db = ROOT / 'resources/corpus/corpus.db'
    con = sqlite3.connect(f'file:{db}?mode=ro', uri=True)
    source = dict(zip(('code','name','license_id','attribution'), con.execute(
        "select code,name,license_id,attribution from source where code='sv1917'").fetchone()))
    rows = []
    for split, refs in SELECTION.items():
        for book, chapter, first, last in refs:
            verses = con.execute('''select v.verse,v.text from verse v join book b on b.id=v.book_id
                join source s on s.id=v.source_id where s.code='sv1917' and b.code=? and
                chapter=? and verse between ? and ? order by verse,verse_suffix''',
                (book,chapter,first,last)).fetchall()
            if len(verses) != last-first+1:
                raise ValueError(f'Missing passage {book} {chapter}:{first}-{last}')
            text = ' '.join(v[1] for v in verses)
            tags = ['short' if len(text.split()) < 17 else 'long' if len(text.split()) > 45 else 'medium']
            import re
            words = re.findall(r'[^\W\d_]+',text.lower())
            for category, values in LEXICAL.items():
                if set(words) & set(values.split()):
                    tags.append(category)
            for tag, pattern in [('sje_tje','sk|sj|stj|skj|tj|kj'),('retroflex','rd|rl|rn|rs|rt'),
                                 ('rounded_vowels','[yöu]'),('clusters','[bcdfghjklmnpqrstvwxz]{3}'),
                                 ('question',r'\?'),('dialogue','»')]:
                if re.search(pattern,text.lower()):
                    tags.append(tag)
            rows.append({'id': f'{book.lower()}-{chapter:03}-{first:03}-{last:03}',
                         'split':split,'reference':f'{book} {chapter}:{first}-{last}',
                         'text':text,'tags':tags})
    legacy = [('psalm','Herren är min herde, mig skall intet fattas. Han låter mig vila på gröna ängar.'),
              ('sjal','Han vederkvicker min själ; han leder mig på rätta vägar, för sitt namns skull.'),
              ('namn','Abraham, Isak och Jakob. Jerusalem och Betlehem. Mose och Aron.')]
    data = {'schema':1,'source':source,'corpus_sha256':hashlib.sha256(db.read_bytes()).hexdigest(),
            'selection':'Fixed curated passages; 40 development, 10 chapter-disjoint heldout; legacy clips separate.',
            'word_categories':{k:v.split() for k,v in LEXICAL.items()}, 'passages':rows,
            'legacy':[{'id':k,'text':t,'split':'diagnostic','tags':['legacy']} for k,t in legacy]}
    Path(__file__).with_name('dataset.json').write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')


if __name__ == '__main__':
    main()
