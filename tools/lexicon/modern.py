"""Import licensed SALDO associations, morphology, and academic word metadata."""
from dataclasses import dataclass
import unicodedata
import xml.etree.ElementTree as ET


def normalized(text):
    return unicodedata.normalize('NFC', text).lower()


def features(element):
    return {x.get('att'): x.get('val') for x in element.findall('feat')}


def lexical_entries(path):
    for _, element in ET.iterparse(path):
        if element.tag == 'LexicalEntry':
            yield element
            element.clear()


@dataclass(frozen=True)
class Entry:
    id: str
    headword: str
    gram: str
    lemgrams: frozenset
    associations: tuple = ()
    forms: frozenset = frozenset()


def whole_word(form):
    code = form['partOfSpeech']
    return (not any(c.isspace() for c in form['writtenForm'])
            and code not in ('sxc', 'mxc')
            and not (len(code) > 2 and code.endswith(('m', 'h'))))


def saldo_entries(path):
    result = {}
    for element in lexical_entries(path):
        forms = [features(x) for x in element.findall('Lemma/FormRepresentation')]
        sense = element.find('Sense')
        if not forms or sense is None:
            continue  # PRIM is a semantic root, not a word.
        single = [form for form in forms if whole_word(form)]
        display = single[0] if single else forms[0]
        relations = tuple(x.get('targets') for x in sense.findall('SenseRelation')
                          if features(x).get('label') in ('primary', 'secondary'))
        entry = Entry(sense.get('id'), display['writtenForm'], display['partOfSpeech'],
                      frozenset(x['lemgram'] for x in single), relations,
                      frozenset(x['writtenForm'] for x in single))
        if entry.id in result:
            raise ValueError(f'Duplicate SALDO sense: {entry.id}')
        result[entry.id] = entry
    return result


def academic_entries(path):
    result = {}
    for element in lexical_entries(path):
        form = element.find('Lemma/FormRepresentation')
        fields = features(form)
        sense = element.find('Sense')
        # The export repeats some sense IDs for distinct ranked entries.
        lemgrams = frozenset(x.get('val') for x in form.findall('feat[@att="lemgram"]')
                            if fields['nativePartOfSpeech'] != 'particip' or '..vb.' not in x.get('val'))
        entry = Entry(sense.get('id') + '#' + fields['rank'], fields['writtenForm'],
                      fields['nativePartOfSpeech'], lemgrams)
        if entry.id in result:
            raise ValueError(f'Duplicate academic word: {entry.id}')
        result[entry.id] = entry
    return result


def morphology(path, words):
    """Keep published full forms for corpus words; exclude compound fragments."""
    result = {}
    for element in lexical_entries(path):
        lemma = features(element.find('Lemma/FormRepresentation'))
        if not whole_word(lemma):
            continue
        candidates = [lemma['writtenForm']]
        for form in element.findall('WordForm'):
            fields = features(form)
            if fields.get('msd') not in ('c', 'ci', 'cm', 'sms'):
                candidates.append(fields['writtenForm'])
        for candidate in candidates:
            word = normalized(candidate)
            if word in words:
                result.setdefault(word, set()).add(lemma['lemgram'])
    return result


def resolve(entries, words, inflections, curated):
    """Retain every whole-word match; never infer a lemma by removing a suffix."""
    direct, lemgrams, headwords = {}, {}, {}
    for key, entry in entries.items():
        # Multiword expressions cannot match one clicked component word.
        for headword in entry.forms or (entry.headword,):
            if any(c.isspace() for c in headword) or entry.gram in ('sxc', 'mxc'):
                continue
            direct.setdefault(normalized(headword), set()).add(key)
            headwords.setdefault(normalized(headword), set()).add(key)
        for lemgram in entry.lemgrams:
            lemgrams.setdefault(lemgram, set()).add(key)
    result = {}
    for word in words:
        matches = {key: 'exact' for key in direct.get(word, ())}
        for lemgram in inflections.get(word, ()):
            for key in lemgrams.get(lemgram, ()):
                matches.setdefault(key, 'inflected')
        if not matches and word in curated:
            headword, verbs = curated[word]
            matches = {key: 'curated' for key in headwords.get(normalized(headword), ())
                       if not verbs or entries[key].gram in ('vb', 'verb')}
        result[word] = matches
    return result


def associations(entry, entries):
    # Association targets are senses, not definitions or claims of synonymy.
    return ', '.join(dict.fromkeys(entries[key].headword for key in entry.associations if key in entries))
