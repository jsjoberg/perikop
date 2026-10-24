#!/usr/bin/env -S uv run --locked
"""Compare the Swedish 1917 corpus with a downloaded MySword SQLite module.

Formatting, headings and footnotes are excluded; verse coordinates are not remapped.
Differences are review candidates, never instructions to replace Scripture text.
"""
import argparse, collections, difflib, hashlib, html, json, re, sqlite3
from pathlib import Path

root=Path(__file__).resolve().parents[2]

def plain(text):
    for tag in ('TS','RF'):
        text=re.sub(fr'<{tag}>.*?<{tag}>','',text,flags=re.I|re.S)
    text=re.sub(r'<(?:CL|br)\s*/?>',' ',text,flags=re.I)
    return ' '.join(html.unescape(re.sub(r'<[^>]+>','',text)).replace(r'\xa0',' ').split())

def words(text):
    return re.findall(r'[^\W\d_]+|\d+',text.lower())

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('module',type=Path)
    parser.add_argument('--corpus',type=Path,default=root/'resources/corpus/corpus.db')
    parser.add_argument('--output',type=Path,default=root/'build/corpus/audit')
    args=parser.parse_args()
    # MySword uses KJV book numbers, including a different Hebrews-to-Jude order.
    names={row[2]:row[0] for row in (line.split('\t') for line in (root/'resources/corpus/books.tsv').read_text().splitlines() if line and not line.startswith('#'))}
    books=[names[b['name']] for b in json.loads((root/'resources/corpus/input/KJV.json').read_text())['books']]
    with sqlite3.connect(f'file:{args.corpus.resolve()}?mode=ro',uri=True) as db:
        ours={(b,c,v):t for b,c,v,t in db.execute('SELECT book.code,chapter,verse,text FROM verse JOIN book ON book.id=book_id WHERE source_id=1') if b in books}
    with sqlite3.connect(f'file:{args.module.resolve()}?mode=ro',uri=True) as db:
        other={}
        for b,c,v,text in db.execute('SELECT Book,Chapter,Verse,Scripture FROM Bible'):
            if not 1<=b<=len(books):raise ValueError('Unexpected MySword book number: '+str(b))
            if not (text:=plain(text)):continue
            key=(books[b-1],c,v)
            if key in other:raise ValueError('Duplicate MySword coordinate: '+str(key))
            other[key]=text
        title,version,date,comments=db.execute('SELECT Title,Version,VersionDate,Comments FROM Details').fetchone()
    shared=ours.keys()&other.keys()
    if len(shared)<30000:raise ValueError('Expected a complete Swedish Bible comparison')
    counts=collections.Counter()
    rows=[]
    for key in sorted(shared):
        a,b=ours[key],other[key]
        if a==b:counts['same_text']+=1
        elif words(a)==words(b):counts['same_words']+=1
        else:
            counts['different_words']+=1
            changes=[f'{" ".join(words(a)[i:j]) or "∅"} → {" ".join(words(b)[u:v]) or "∅"}' for tag,i,j,u,v in difflib.SequenceMatcher(None,words(a),words(b),autojunk=False).get_opcodes() if tag!='equal']
            rows.append(('%s %d:%d'%key,'; '.join(changes),a,b))
    summary={
        'module':{'title':title,'version':version,'date':date,'comments':plain(comments or ''),'sha256':hashlib.sha256(args.module.read_bytes()).hexdigest()},
        'corpus_sha256':hashlib.sha256(args.corpus.read_bytes()).hexdigest(),
        'corpus_verses':len(ours),'module_nonempty_verses':len(other),'shared_coordinates':len(shared),**counts,
        'only_corpus':sorted(ours.keys()-other.keys()),'only_module':sorted(other.keys()-ours.keys())}
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/'mysword-summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n')
    (args.output/'mysword-differences.tsv').write_text('reference\tword_changes\tperikop\tmysword\n'+'\n'.join('\t'.join(row) for row in rows)+'\n')
    print(json.dumps({k:v for k,v in summary.items() if k not in ('module','only_corpus','only_module')},ensure_ascii=False))
    print(f'Only corpus: {len(summary["only_corpus"])}; only module: {len(summary["only_module"])}')

if __name__=='__main__':
    assert plain(r'<TS>Title<Ts><CL>\xa0Vi <FI>spärrar<Fi>.<RF>Note<Rf>')=='Vi spärrar.'
    assert words('Vi bida—efter frid.')==['vi','bida','efter','frid']
    main()
