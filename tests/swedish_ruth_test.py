#!/usr/bin/env -S uv run --locked
"""Keep Ruth's restored clauses, verse boundaries and supported print variants."""
import sqlite3
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/corpus'))
from mapping import Mapper
from runeberg1917 import read

text = {key[1:]: value for key, value in read(
    ROOT / 'resources/corpus/input/runeberg-bibeln-1917-html.zip').items() if key[0] == 'Ruth'}
assert list(text) == [(c, v) for c, count in enumerate((22, 23, 18, 22), 1)
                      for v in range(1, count + 1)]
assert 'Vänden om och gån hem igen' in text[1, 8]
assert text[3, 5] == 'Hon svarade henne: »Allt vad du säger vill jag göra.»'
assert ('göra det. Men har han icke lust att taga dig efter bördesrätt, '
        'då skall jag göra det, så sant HERREN lever.') in text[3, 13]
assert text[4, 4].startswith('Därför tänkte jag, att')
assert ('så gör det; men om du icke vill taga det efter bördesrätt, '
        'så säg mig det') in text[4, 4]
assert ('allt vad som har tillhört Elimelek, och allt vad som har tillhört '
        'Kiljon och Mahelon.') in text[4, 9]
# A scan-specific punctuation choice must not silently replace a retained witness.
assert 'vänta, till dess' in text[1, 13]
assert 'Ty dit du går vill ock jag gå' in text[1, 16]
assert 'De svarade honom: »' in text[2, 4]
assert text[2, 13].endswith('tjänarinnor.»')
assert 'morgonen, men hon' in text[3, 14]
assert text[3, 17].endswith("svärmoder.'»")
assert '»Noomi har fått en son»; och' in text[4, 17]

with sqlite3.connect((ROOT / 'resources/corpus/corpus.db').as_uri() + '?mode=ro', uri=True) as db:
    stored = {(c, v): value for c, v, value in db.execute(
        "SELECT v.chapter,v.verse,v.text FROM verse v JOIN source s ON s.id=v.source_id "
        "JOIN book b ON b.id=v.book_id WHERE s.code='sv1917' AND b.code='Ruth' "
        "ORDER BY v.chapter,v.verse")}
    assert stored == text
    with sqlite3.connect((ROOT / 'resources/corpus/osb-reference.db').as_uri() + '?mode=ro', uri=True) as ref:
        mapper = Mapper(db, ref)
        for c, v in text:
            coordinate = (c, v, '')
            for a, b in (('sv1917', 'osb-v1'), ('osb-v1', 'sv1917')):
                assert mapper.counterparts(a, b, 'Ruth', coordinate) == [('Ruth', coordinate)]
print('Ruth: 85 verse coordinates, restored clauses, retained variants and 170 mapping directions passed')
