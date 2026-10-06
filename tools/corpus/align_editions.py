#!/usr/bin/env -S uv run --locked
"""Generate verse alignments between Scripture editions with different versifications.

Reads verse texts from resources/corpus/corpus.db, so run build_corpus.py first, and
writes resources/corpus/alignment.tsv, which build_corpus.py imports on its next run.
A Gale-Church dynamic program aligns verse lengths and shared proper names. Each book
pair has explicit numbering rules: these state where an edition follows another chapter
division, and the program prefers alignments that agree with them. Overrides state
transpositions, which a monotonic alignment cannot express. Review the rules with
--report before committing a regenerated table.
"""
import collections, math, re, sqlite3, sys, unicodedata
from pathlib import Path
root=Path(__file__).resolve().parents[2]
from align_rules import UNITS, RULES, OVERRIDES, PAIRS, OLD_TESTAMENT

def load(db,source,book):
    query='SELECT chapter,verse,verse_suffix,text FROM verse JOIN source ON source.id=source_id JOIN book ON book.id=book_id WHERE source.code=? AND book.code=? ORDER BY chapter,verse,verse_suffix'
    return [((book,c,v,s),t) for c,v,s,t in db.execute(query,(source,book))]

# Proper names make the strongest cross-language anchors. Compare consonant skeletons.
GREEK=dict(zip('αβγδεζηθικλμνξοπρσςτυφχψω',['a','b','g','d','e','z','e','th','i','k','l','m','n','x','o','p','r','s','s','t','u','f','ch','ps','o']))
COMMON=set('herren herre gud guds lord god och men ty sa da jag du han hon vi de den det detta dessa om nar se nu ja nej dar the and but for thou thee thy ye he she it they we then now behold when therefore wherefore yea who what why how in of to that this these those which kyrios kyriou kyrio kyrion kyrie theos theou theo theon thee kai idou oti outos tade diatouto ean ei ou os en epi ego sy autos ouk me de gar'.split())
def latin(word):
    word=unicodedata.normalize('NFD',word.lower())
    word=''.join(GREEK.get(ch,ch) for ch in word if not unicodedata.combining(ch))
    return word.translate(str.maketrans('åäöéü','aaoeu'))
def skeleton(word):
    word=latin(word)
    for old,new in (('ph','f'),('th','t'),('ch','k'),('ck','k'),('c','k'),('q','k'),('x','ks'),('z','s'),('w','v'),('j','i'),('y','i'),('h','')):
        word=word.replace(old,new)
    return re.sub(r'(.)\1+',r'\1',re.sub(r'[^a-z]|[aeiou]','',word))[:3]
def names(text):
    found=[]
    for match in re.finditer(r'(?<![.;:!?·»«“])\s+([^\W\d_][\ẁ-ͯ]{2,})',' '+text):
        word=match[1]
        if word[0].isupper() and re.sub(r'[^a-z]','',latin(word)) not in COMMON and not (word.isupper() and len(word)>4):
            stem=skeleton(word)
            if len(stem)>=2:found.append(stem)
    return found
def letters(text):return len(re.findall(r'[^\W\d_]',text))

BEADS={(1,1):0.86,(1,0):0.03,(0,1):0.03,(2,1):0.035,(1,2):0.035,(2,2):0.005,(3,1):0.003,(1,3):0.003}
def align(src,tgt,expect,band=160,variance=12.0,name_weight=1.5,expect_weight=4.0):
    """Monotonic alignment of two verse sequences; returns [(src refs, tgt refs)]."""
    n,m=len(src),len(tgt)
    ls=[letters(t) for _,t in src];lt=[letters(t) for _,t in tgt]
    ratio=(sum(lt) or 1)/(sum(ls) or 1)
    ns=[set(names(t)) for _,t in src];nt=[set(names(t)) for _,t in tgt]
    frequency=collections.Counter(s for group in ns+nt for s in group)
    weight={s:min(1.5,math.log((n+m)/c)) for s,c in frequency.items()}
    # Lettered verses are additions; their number says nothing about the source verse.
    wanted=[expect(ref) for ref,_ in src];have=[None if ref[3] else ref[:3] for ref,_ in tgt]
    def cost(i0,i1,j0,j1):
        di,dj=i1-i0,j1-j0;a=sum(ls[i0:i1]);b=sum(lt[j0:j1])
        if not di or not dj:return -math.log(BEADS[(di,dj)])+0.004*(a+b)
        d=(b-a*ratio)/math.sqrt(max(a,1)*variance*ratio+1)
        c=-math.log(max(math.erfc(abs(d)/math.sqrt(2)),1e-12))-math.log(BEADS[(di,dj)])
        sn=set().union(*ns[i0:i1]);tn=set().union(*nt[j0:j1])
        c-=name_weight*sum(weight[s] for s in sn&tn)
        c+=0.3*name_weight*sum(min(1.0,weight[s]) for s in sn^tn if frequency[s]<=6)
        hits=len({wanted[k] for k in range(i0,i1)}&set(have[j0:j1]))
        return c-expect_weight*hits/max(di,dj)
    best={(0,0):(0.0,None)}
    for i in range(n+1):
        centre=i*m/max(n,1)
        for j in range(max(0,int(centre-band)),min(m,int(centre+band))+1):
            if i==j==0:continue
            options=[(best[(i-di,j-dj)][0]+cost(i-di,i,j-dj,j),(i-di,j-dj)) for di,dj in BEADS if (i-di,j-dj) in best]
            if options:best[(i,j)]=min(options)
    if (n,m) not in best:raise ValueError('alignment band too narrow')
    beads=[];i,j=n,m
    while (i,j)!=(0,0):
        pi,pj=best[(i,j)][1];beads.append(([r for r,_ in src[pi:i]],[r for r,_ in tgt[pj:j]]));i,j=pi,pj
    return beads[::-1]

REF=re.compile(r'(\w+) (\d+):(\d+)([a-z]?)')
def parse_ref(text):
    m=REF.fullmatch(text);return (m[1],int(m[2]),int(m[3]),m[4])
def rule_table(rules,book):
    """Expected target (book, chapter, verse) for a source verse under the numbering rules."""
    parsed=[]
    for rule in rules:
        left,right=rule.split('=')
        sb,span=left.split(' ');tb,start=(right.split(' ') if ' ' in right else (sb,right))
        if right=='-':tb,start=None,'0:0' if ':' in span else '0'
        if ':' in span:
            c,vs=span.split(':');v1,v2=(vs.split('-')+[vs])[:2];tc,tv=map(int,start.split(':'))
            parsed.append((sb,int(c),int(c),int(v1),int(v2),tb,tc-int(c),tv-int(v1)))
        else:
            c1,c2=(span.split('-')+[span])[:2]
            parsed.append((sb,int(c1),int(c2),1,999,tb,int(start)-int(c1),0))
    def expect(ref):
        b,c,v,_=ref
        for sb,c1,c2,v1,v2,tb,dc,dv in parsed:
            if sb==b and c1<=c<=c2 and v1<=v<=v2:return (tb,c+dc,v+dv) if tb else None
        return (book,c,v)
    return expect
def select(rows,span):
    if span is None:return rows
    m=re.fullmatch(r'(\d+)(?::(\d+))?(?:-(\d+)(?::(\d+))?)?',span)
    c1=int(m[1]);v1=int(m[2] or 1)
    if m[3] and m[4]:c2,v2=int(m[3]),int(m[4])
    elif m[3] and m[2]:c2,v2=c1,int(m[3])
    elif m[3]:c2,v2=int(m[3]),999
    else:c2,v2=c1,int(m[2] or 999)
    return [r for r in rows if (c1,v1)<=(r[0][1],r[0][2])<=(c2,v2)]

def expand(text):
    """Refs in an override side: 'Book c:v', 'Book c:v1-v2' or '-'."""
    refs=[]
    for book,c,v1,v2,suffix in re.findall(r'(\w+) (\d+):(\d+)(?:-(\d+))?([a-z]?)',text):
        refs+=[(book,int(c),v,suffix) for v in range(int(v1),int(v2 or v1)+1)]
    return refs
def pairs_for(db,pair,unit):
    """Verse-level links for one alignment unit, after overrides."""
    src_code,tgt_code=pair
    spec=UNITS.get(pair,{}).get(unit,{})
    src_parts=spec.get('src',[(unit,None)]);tgt_parts=spec.get('tgt',[(unit,None)])
    cache={}
    def rows(code,book):
        if (code,book) not in cache:cache[(code,book)]=load(db,code,book)
        return cache[(code,book)]
    src=[r for b,span in src_parts for r in select(rows(src_code,b),span)]
    tgt=[r for b,span in tgt_parts for r in select(rows(tgt_code,b),span)]
    if not src or not tgt:return [],src,tgt
    expect=rule_table(RULES.get(pair,{}).get(unit,[]),tgt_parts[0][0])
    beads=align(src,tgt,expect)
    fixed=[];touched_src=set();touched_tgt=set()
    for line in OVERRIDES.get(pair,{}).get(unit,[]):
        pairwise='>>' in line
        left,right=[expand(x.strip()) for x in line.split('>>' if pairwise else '>')]
        links=list(zip(left,right)) if pairwise else [(left,right)] if left else [([],[r]) for r in right]
        if pairwise and len(left)!=len(right):raise ValueError('Unequal override '+line)
        if not pairwise and left and not right:links=[([r],[]) for r in left]
        for s,t in links:
            s,t=([s],[t]) if pairwise else (s,t)
            fixed.append((s,t));touched_src.update(s);touched_tgt.update(t)
    beads=[([r for r in s if r not in touched_src],[r for r in t if r not in touched_tgt]) for s,t in beads]
    return [b for b in beads if b[0] or b[1]]+fixed,src,tgt

def fmt(ref):return '%s %d:%d%s'%ref if ref else '-'
def rows_for(beads,src_order,tgt_order,missing):
    """Compress verse links into rows (from first, from last, to first, to last, kind).

    Kinds follow AlignmentKind: 0 same, 1 renumbered, 2 split, 3 merged, 5 LXX only, 6 MT only."""
    def contiguous(refs,order):return all(order.get(b,-2)==order.get(a,-9)+1 for a,b in zip(refs,refs[1:]))
    rows=[];singles=[]
    for s,t in beads:
        if t and not contiguous(t,tgt_order):t=t[:1] # A transposed second part is reachable from its own verse.
        for s in ([[r] for r in s] if not contiguous(s,src_order) else [s]):
            if len(s)==len(t)>1:singles+=list(zip(s,t))
            elif len(s)==1 and len(t)==1:singles.append((s[0],t[0]))
            elif not t:rows+=[(r,r,None,None,missing) for r in s]
            elif len(s)==1:rows.append((s[0],s[0],t[0],t[-1],2))
            else:rows.append((s[0],s[-1],t[0],t[0],3))
    singles.sort(key=lambda p:src_order[p[0]])
    merged=[]
    for a,b in singles:
        if merged and not a[3] and not b[3]:
            f0,f1,t0,t1,k=merged[-1]
            if f1[0]==a[0] and t1[0]==b[0] and not f1[3] and not t1[3] and (
                (f1[1]==a[1] and t1[1]==b[1] and a[2]==f1[2]+1 and b[2]==t1[2]+1) or
                (a[1]==f1[1]+1 and b[1]==t1[1]+1 and a[2]==b[2]==1 and f0[2]==t0[2])):
                merged[-1]=(f0,a,t0,b,k);continue
        merged.append((a,a,b,b,0 if a==b else 1))
    # Runs of verses without counterpart share one row.
    missing_rows=[]
    for row in sorted((r for r in rows if r[2] is None),key=lambda r:src_order[r[0]]):
        if missing_rows and missing_rows[-1][1][0]==row[0][0] and src_order[row[0]]==src_order[missing_rows[-1][1]]+1:
            missing_rows[-1]=(missing_rows[-1][0],row[1],None,None,missing)
        else:missing_rows.append(row)
    return sorted([r for r in rows if r[2] is not None]+missing_rows+merged,key=lambda r:src_order[r[0]])

def links_of(beads):
    """{source verse: target verses} for every source verse in the beads."""
    out={}
    for s,t in beads:
        for r in s:out.setdefault(r,[]);out[r]+=[x for x in t if x not in out[r]]
    return out
def compose(first,second):
    return {a:list(dict.fromkeys(c for b in bs for c in second.get(b,[]))) for a,bs in first.items()}
def beads_of(links,order):
    """Regroup links; neighbouring verses with one shared target form a merge."""
    beads=[]
    for ref,targets in sorted(links.items(),key=lambda item:order[item[0]]):
        if beads and targets and beads[-1][1]==targets:beads[-1][0].append(ref)
        else:beads.append(([ref],list(targets)))
    return beads

def generate(report=False):
    db=sqlite3.connect(root/'resources/corpus/corpus.db')
    orders={}
    def order(code):
        if code not in orders:
            query='SELECT book.code,chapter,verse,verse_suffix FROM verse JOIN source ON source.id=source_id JOIN book ON book.id=book_id WHERE source.code=? ORDER BY book.canonical_order,chapter,verse,verse_suffix'
            orders[code]={tuple(r):i for i,r in enumerate(db.execute(query,(code,)))}
        return orders[code]
    links=collections.defaultdict(dict)
    for pair,units in PAIRS.items():
        for unit in units:
            beads,src,tgt=pairs_for(db,pair,unit)
            if not beads:continue
            keep=UNITS.get(pair,{}).get(unit,{}).get('keep')
            links[pair].update({r:t for r,t in links_of(beads).items() if not keep or r[0] in keep})
            links[pair[::-1]].update(links_of([(t,s) for s,t in beads]))
            if report:
                print('##',pair,unit,len(src),len(tgt))
                expect=rule_table(RULES.get(pair,{}).get(unit,[]),UNITS.get(pair,{}).get(unit,{}).get('tgt',[(unit,None)])[0][0])
                for s,t in beads:
                    if len(s)==1 and len(t)==1 and expect(s[0])==t[0][:3] and not t[0][3]:continue
                    print('  ',' '.join(fmt(r) for r in s) or '-','>',' '.join(fmt(r) for r in t) or '-')
    # WEB numbers the Old Testament like the KJV.
    for a,b in (('en-kjv','sv1917'),('sv1917','en-kjv')):
        a2,b2=[('en-web' if code=='en-kjv' else code) for code in (a,b)]
        for ref,targets in links[(a,b)].items():
            if ref[0] in OLD_TESTAMENT and all(t[0] in OLD_TESTAMENT for t in targets):links[(a2,b2)].setdefault(ref,targets)
    # English editions reach the Septuagint through the reviewed Swedish links.
    for english in ('en-kjv','en-web'):
        for a,b in ((english,'grc-lxx'),('grc-lxx',english)):
            links[(a,b)]={**compose(links[(a,'sv1917')],links[('sv1917',b)]),**links[(a,b)]}
    out=[]
    for (a,b),table in sorted(links.items()):
        table={r:t for r,t in table.items() if r in order(a)}
        for row in rows_for(beads_of(table,order(a)),order(a),order(b),5 if a=='grc-lxx' else 6):out.append((a,b)+row)
    return out

if __name__=='__main__':
    if '--report' in sys.argv:generate(report=True)
    else:
        rows=generate()
        def ref(r):return '%d:%d%s'%r[1:] if r else '-'
        lines=['%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%d'%(a,b,f0[0],ref(f0),ref(f1),t0[0] if t0 else '-',ref(t0),ref(t1),k) for a,b,f0,f1,t0,t1,k in rows]
        (root/'resources/corpus/alignment.tsv').write_text('\n'.join(lines)+'\n')
        print('Wrote',len(lines),'alignment rows')
