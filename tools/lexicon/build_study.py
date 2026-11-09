#!/usr/bin/env python3
"""Fill the corpus database's word-study tables for the Ordstudium panel from pinned inputs.

tools/corpus/build_corpus.py calls build() on the database it is building.
Swedish 1917 forms resolve to Dalin, Nyström, and SALDO entries at build time,
and each keeps only its kind, base form, a one-line gloss and a link. The reader only looks forms up.
"""
import itertools, re
import xml.etree.ElementTree as ET
from pathlib import Path
from fetch import fetch
from biblobok import articles, resolve_forms
from modern import saldo_entries, morphology, resolve as resolve_modern, associations
root=Path(__file__).resolve().parents[2]
CLASSES={'nn':'substantiv','av':'adjektiv','vb':'verb','ab':'adverb','pm':'egennamn','pn':'pronomen','pp':'preposition',
         'kn':'konjunktion','sn':'subjunktion','in':'interjektion','nl':'räkneord','al':'artikel','nna':'förkortning','pma':'förkortning','vba':'förkortning'}

def build(db, additional_words=None):
    """Resolve the Swedish 1917 words in db's verse table and write the word-study tables.

    additional_words counts further words as written, so a capital still tells a name."""
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
    counts={};uncapitalized=set()
    for (text,) in db.execute("SELECT text FROM verse JOIN source ON source.id=source_id WHERE source.code='sv1917'"):
        for word in re.findall(r"[^\W\d_]+",text):
            counts[word.lower()]=counts.get(word.lower(),0)+1
            if word[0].islower():uncapitalized.add(word)
    for word,count in (additional_words or {}).items():
        counts[word.lower()]=counts.get(word.lower(),0)+count
        if word[0].islower():uncapitalized.add(word)
    resolved={word:resolve(word) for word in counts}
    biblical_entries=articles()
    biblical_forms=resolve_forms(biblical_entries,counts,spellings)
    biblical_used={article for matches in biblical_forms.values() for article in matches}
    saldo=saldo_entries(fetch('saldo'))
    inflections=morphology(fetch('saldom'),counts)
    saldo_forms=resolve_modern(saldo,counts,inflections,irregular)
    saldo_used={key for matches in saldo_forms.values() for key in matches}

    # --- Write ---------------------------------------------------------------------
    # One sense per meaning: its kind (word class or biblical entry), base form, one-line gloss and link.
    # A form lists its senses in order: biblical articles, then SALDO's senses with its own spelling first, and Dalin's entries only
    # where SALDO has none, because Dalin's old spelling also matches unrelated words (han: hane).
    def short(text,limit=160):
        text=' '.join(text.split())
        return text if len(text)<=limit else text[:limit].rsplit(' ',1)[0].rstrip(',;:')+'…'
    def first_sense(definition):
        return short(re.sub(r'^1\. ','',definition.split('\n')[0]))
    def word_class(lemgram):
        code=lemgram.split('..')[-1].split('.')[0]
        return CLASSES.get(code,code)
    biblical={key:(headword,short(definition),url) for key,headword,_,definition,url,_ in biblical_entries}
    senses={};words=[]
    # Name aliases such as pauli have an article although the 1917 text lacks them.
    for word in sorted(set(counts)|set(biblical_forms)):
        rows=[]
        # A name: SALDO lists one, or the text never writes the word without a capital.
        names=word not in uncapitalized or any(saldo[key].gram=='pm' for key in saldo_forms.get(word,()))
        for article in biblical_forms.get(word,()):
            headword,gloss,url=biblical[article]
            rows.append(('egennamn' if names else 'bibliskt uppslagsord',headword,gloss,url))
        modern=sorted(saldo_forms.get(word,{}),key=lambda key:(saldo_forms.get(word,{})[key]!='exact',key))
        classes=[CLASSES.get(saldo[key].gram,saldo[key].gram) for key in modern]
        old=[word_class(l) for l in sorted(resolved.get(word,()))]
        for key,kind in zip(modern,classes):
            entry=saldo[key]
            # The biblical article already describes the name.
            if entry.gram=='pm' and biblical_forms.get(word,()):continue
            # Dalin's definition only where one Dalin entry and one SALDO sense of a class are the same word.
            gloss=''
            if classes.count(kind)==1 and old.count(kind)==1:
                headword,_,definition=entries[sorted(resolved.get(word,()))[old.index(kind)]]
                if headword.lower() in spellings(entry.headword.lower()):gloss=first_sense(definition)
            related=associations(entry,saldo)
            rows.append((kind,entry.headword,gloss or (related and 'Jfr '+related),''))
        if not saldo_forms.get(word,{}):
            rows+=[(word_class(l),entries[l][0],first_sense(entries[l][2]),'') for l in sorted(resolved.get(word,()))]
        for rank,row in enumerate(dict.fromkeys(rows)):
            words.append((word,rank,senses.setdefault(row,len(senses)+1)))
    db.executemany('INSERT INTO sense VALUES(?,?,?,?,?)',[(i,*row) for row,i in senses.items()])
    # --- How each sense sounds, where a form's senses differ ------------------------
    by_id={i:row for row,i in senses.items()}
    said={};usual={}  # form -> the phonemes read-aloud can give it; form -> the word's own
    for line in (root/'resources/speech/lexicon.tsv').read_text(encoding='utf-8').splitlines():
        form,codes=line.split('\t');usual[form]=codes.replace(' ','')
    for line in (root/'resources/corpus/pronunciation-sv.tsv').read_text(encoding='utf-8').splitlines():
        fields=line.split('\t')
        if line.startswith('#') or len(fields)<2 or fields[2:]==['spoken']:continue
        bracket=re.search(r'\[(.+?)\]',fields[0])
        form=(bracket[1] if bracket else fields[0]).lower()
        said.setdefault(form,set()).add(fields[1])
        if not bracket:usual[form]=fields[1]
    phonemes={}
    for line in (root/'resources/lexicon/sv1917-sense-pronunciations.tsv').read_text(encoding='utf-8').splitlines():
        if not line or line.startswith('#'):continue
        form,kind,headword,codes,*gloss=line.split('\t')
        ranks=[rank for word,rank,sense in words if word==form and by_id[sense][:2]==(kind,headword) and gloss in ([],[by_id[sense][2]])]
        if len(ranks)!=1:raise SystemExit(f'sv1917-sense-pronunciations.tsv: {form} {kind} {headword} names {len(ranks)} senses')
        if codes not in said.get(form,set())|{usual.get(form)}:raise SystemExit(f'sv1917-sense-pronunciations.tsv: read-aloud never says {form} as {codes}')
        phonemes[form,ranks[0]]=(codes,int(codes==usual.get(form)))
    db.executemany('INSERT INTO word VALUES(?,?,?,?,?)',[(*word,*phonemes.get(word[:2],('',0))) for word in words])
    print(f'Sense pronunciations: {len(phonemes)} senses of {len({form for form,_ in phonemes})} forms')
    if db.execute('PRAGMA foreign_key_check').fetchall():raise SystemExit('Word-study foreign key check failed')
    total=sum(counts.values());covered=sum(n for w,n in counts.items() if resolved[w])
    print(f'Swedish forms: {sum(1 for w in counts if resolved[w])}/{len(counts)} resolved, {100*covered/total:.1f}% of word occurrences')
    combined=sum(n for word,n in counts.items() if resolved[word] or biblical_forms[word])
    print(f'Biblical articles: {len(biblical_used)} of {len(biblical_entries)}; combined Swedish lookup coverage: {100*combined/total:.1f}% of word occurrences')
    print(f'Senses: {len(senses)} for {len({w for w,*_ in words})} forms')
    print(f'SALDO senses: {len(saldo_used)}')
    modern_covered=sum(n for word,n in counts.items() if saldo_forms[word])
    all_covered=sum(n for word,n in counts.items() if resolved[word] or biblical_forms[word] or saldo_forms[word])
    print(f'SALDO: {100*modern_covered/total:.1f}%; all sources: {100*all_covered/total:.1f}% of word occurrences')
