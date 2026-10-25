#!/usr/bin/env -S uv run --locked
"""Write a review queue for Swedish OCR/transcription candidates; never edit Scripture.

Run after build_corpus.py. Lower priorities are reviewed first. Suggestions are
corpus neighbours, not corrections: names, old inflections and printing variants
need the same scan check as apparent OCR errors. Statistics describe candidates,
not measured error rates. Output: build/corpus/sv1917-candidates.tsv.
"""
import collections
import csv
import re
import sqlite3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WORD = re.compile(r'[^\W\d_]+')
CONFUSIONS = [('rn', 'm'), ('i', 'l'), ('l', 'i'), ('r', 't'),
              ('å', 'ä'), ('ä', 'å'), ('a', 'å'), ('å', 'a'),
              ('a', 'ä'), ('ä', 'a'), ('o', 'ö'), ('ö', 'o')]
PUNCTUATION = set(' ,.;:!?»–—-\'’()[]0123456789')


def neighbours(word, counts):
    """Replace one OCR-confusable unit; repeated errors can still occur five times."""
    if counts[word] > 5:
        return []
    found = set()
    for old, new in CONFUSIONS:
        start = 0
        while (pos := word.find(old, start)) >= 0:
            other = word[:pos] + new + word[pos + len(old):]
            if counts[other] >= max(5, 5 * counts[word]):
                found.add(other)
            start = pos + 1
    return sorted(found)


def candidates(text, counts, known):
    tokens = list(WORD.finditer(text))
    for i, match in enumerate(tokens):
        form = match[0]
        word = form.lower()
        reasons, suggestions = [], neighbours(word, counts)
        if suggestions:
            reasons.append('known-confusable' if word in known else 'confusable')
        # Swedish I (you), A and O, vocative o, preposition å, noun ö, and u-farsin are real.
        if len(word) == 1 and word not in ('i', 'o', 'å', 'ö'):
            if not (form == 'A' and 'A och O' in text):
                if not (word in ('s', 'v') and re.search(r'o\.\s*s\.\s*v\.', text)):
                    if not (word == 'u' and text[match.start():].startswith('u-farsin')):
                        reasons.append('single-letter')
        if re.search(r'[a-zåäö][A-ZÅÄÖ]', form):
            reasons.append('internal-capital')
        if re.search(r'(.)\1\1', word):
            reasons.append('repeated-letter')
        if i + 1 < len(tokens):
            following = tokens[i + 1]
            if re.fullmatch(r'\s+', text[match.end():following.start()]):
                joined = word + following[0].lower()
                if (min(len(word), len(following[0])) <= 2 and counts[joined] >= 2
                        and (word not in known or following[0].lower() not in known)):
                    reasons.append('possible-split')
                    suggestions.append(joined)
        if counts[word] <= 2 and word not in known:
            reasons.append('rare-unlisted')
        if reasons:
            if any(r in reasons for r in ('single-letter', 'internal-capital', 'repeated-letter')):
                priority = 1
            elif 'confusable' in reasons:
                priority = 2
            else:
                priority = 3 if any(r in reasons for r in ('possible-split', 'known-confusable')) else 4
            yield priority, form, ','.join(reasons), ','.join(sorted(set(suggestions)))
    for char in sorted(set(text)):
        if not char.isalpha() and not char.isspace() and char not in PUNCTUATION:
            yield 1, char, 'unusual-character', ''


def audit():
    with sqlite3.connect((ROOT / 'resources/corpus/corpus.db').as_uri() + '?mode=ro', uri=True) as db:
        rows = db.execute("""SELECT b.code,v.chapter,v.verse,v.text,
            CASE WHEN b.deuterocanonical THEN 'AP' WHEN b.new_testament THEN 'NT' ELSE 'GT' END
            FROM verse v JOIN book b ON b.id=v.book_id JOIN source s ON s.id=v.source_id
            WHERE s.code='sv1917' ORDER BY b.canonical_order,v.chapter,v.verse""").fetchall()
        known = {form.lower() for table in ('sv_word', 'sv_saldo', 'sv_biblical')
                 for (form,) in db.execute('SELECT DISTINCT form FROM ' + table)}
    if not rows:
        raise ValueError('No sv1917 verses; build the corpus first')
    counts = collections.Counter(m[0].lower() for row in rows for m in WORD.finditer(row[3]))
    known.update(('i', 'o', 'å', 'ö', 'a', 's', 'v', 'u'))
    stats = collections.defaultdict(collections.Counter)
    queue = []
    for book, chapter, verse, text, section in rows:
        stats[section]['verses'] += 1
        stats[section]['words'] += len(WORD.findall(text))
        hits = list(candidates(text, counts, known))
        stats[section]['flagged_verses'] += bool(hits)
        stats[section]['priority_1_2_verses'] += any(hit[0] <= 2 for hit in hits)
        for priority, form, reason, suggestion in hits:
            queue.append((priority, section, book, chapter, verse, form, counts[form.lower()], reason, suggestion, text))
    # Stable corpus order within a priority makes adjacent page checks practical.
    queue.sort(key=lambda row: row[0])
    output = ROOT / 'build/corpus/sv1917-candidates.tsv'
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('w', encoding='utf-8', newline='') as handle:
        writer = csv.writer(handle, delimiter='\t', lineterminator='\n')
        writer.writerow(('priority', 'section', 'book', 'chapter', 'verse', 'form', 'occurrences', 'reason', 'suggestion', 'context'))
        writer.writerows(queue)
    print('section\tverses\twords\tflagged_verses\tpriority_1_2_verses\tpriority_1_2_per_10000_words')
    for section in ('NT', 'GT', 'AP'):
        s = stats[section]
        print('%s\t%d\t%d\t%d\t%d\t%.2f' % (section, s['verses'], s['words'], s['flagged_verses'], s['priority_1_2_verses'], 10000 * s['priority_1_2_verses'] / s['words']))
    print('Wrote', len(queue), 'candidates to', output.relative_to(ROOT))


if __name__ == '__main__':
    audit()
