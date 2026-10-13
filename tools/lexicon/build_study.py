#!/usr/bin/env python3
"""Fill the corpus database's word-study tables for the Ordstudium panel from pinned inputs.

tools/corpus/build_corpus.py calls build() on the database it is building.
Swedish 1917 forms resolve to Dalin, Nyström, SALDO, and academic word entries
at build time. The reader only looks forms up.
"""
import itertools, re
import xml.etree.ElementTree as ET
from pathlib import Path
from fetch import fetch
from biblobok import articles, resolve_forms
from modern import saldo_entries, academic_entries, morphology, resolve as resolve_modern, associations
root=Path(__file__).resolve().parents[2]

def build(db):
    """Resolve the Swedish 1917 words in db's verse table and write the word-study tables."""
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
    counts={}
    for (text,) in db.execute("SELECT text FROM verse JOIN source ON source.id=source_id WHERE source.code='sv1917'"):
        for word in re.findall(r"[^\W\d_]+",text):counts[word.lower()]=counts.get(word.lower(),0)+1
    resolved={word:resolve(word) for word in counts}
    biblical_entries=articles()
    biblical_forms=resolve_forms(biblical_entries,counts,spellings)
    biblical_used={article for matches in biblical_forms.values() for article in matches}
    saldo=saldo_entries(fetch('saldo'))
    academic=academic_entries(fetch('ao'))
    inflections=morphology(fetch('saldom'),counts)
    saldo_forms=resolve_modern(saldo,counts,inflections,irregular)
    academic_forms=resolve_modern(academic,counts,inflections,irregular)
    saldo_used={key for matches in saldo_forms.values() for key in matches}
    academic_used={key for matches in academic_forms.values() for key in matches}

    # --- Write ---------------------------------------------------------------------
    ids={lemgram:i for i,lemgram in enumerate(sorted({l for ls in resolved.values() for l in ls}),1)}
    db.executemany('INSERT INTO dalin VALUES(?,?,?,?)',[(i,*entries[l]) for l,i in ids.items()])
    db.executemany('INSERT INTO sv_word VALUES(?,?)',sorted((word,ids[l]) for word,ls in resolved.items() for l in ls))
    db.executemany('INSERT INTO biblical VALUES(?,?,?,?)',sorted((key,headword,definition,url)
        for key,headword,_,definition,url,_ in biblical_entries if key in biblical_used))
    db.executemany('INSERT INTO sv_biblical VALUES(?,?,?)',sorted((word,article,rank) for word,matches in biblical_forms.items() for rank,article in enumerate(matches)))
    db.executemany('INSERT INTO saldo VALUES(?,?,?,?)',[(key,saldo[key].headword,saldo[key].gram,associations(saldo[key],saldo)) for key in sorted(saldo_used)])
    db.executemany('INSERT INTO academic VALUES(?,?,?)',[(key,academic[key].headword,academic[key].gram) for key in sorted(academic_used)])
    for table,matches in (('sv_saldo',saldo_forms),('sv_academic',academic_forms)):
        db.executemany(f'INSERT INTO {table} VALUES(?,?,?)',sorted((word,key,match) for word,entries in matches.items() for key,match in entries.items()))
    if db.execute('PRAGMA foreign_key_check').fetchall():raise SystemExit('Word-study foreign key check failed')
    total=sum(counts.values());covered=sum(n for w,n in counts.items() if resolved[w])
    print(f'Swedish forms: {sum(1 for w in counts if resolved[w])}/{len(counts)} resolved, {100*covered/total:.1f}% of word occurrences')
    combined=sum(n for word,n in counts.items() if resolved[word] or biblical_forms[word])
    print(f'Biblical articles: {len(biblical_used)} of {len(biblical_entries)}; combined Swedish lookup coverage: {100*combined/total:.1f}% of word occurrences')
    print(f'Dalin entries: {len(ids)}')
    print(f'SALDO senses: {len(saldo_used)}; academic words: {len(academic_used)}')
    modern_covered=sum(n for word,n in counts.items() if saldo_forms[word] or academic_forms[word])
    all_covered=sum(n for word,n in counts.items() if resolved[word] or biblical_forms[word] or saldo_forms[word] or academic_forms[word])
    print(f'Modern sources: {100*modern_covered/total:.1f}%; all sources: {100*all_covered/total:.1f}% of word occurrences')
