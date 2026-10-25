#!/usr/bin/env -S uv run --locked
"""OCR candidate rules must find corruption without rewriting historical Swedish."""
import collections
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/corpus'))
from audit_swedish import candidates, neighbours

counts = collections.Counter({'som': 100, 'sorn': 2, 'läppar': 30, 'låppar': 2,
                              'framhovar': 2, 'avbrott': 3})
known = {'i', 'o', 'å', 'a', 'och', 'är', 'u', 's', 'v', 'ö'}
assert neighbours('sorn', counts) == ['som']
assert neighbours('låppar', counts) == ['läppar']
assert not neighbours('som', counts)
assert not neighbours('sorn', counts | {'sorn': 6})
assert any(row[1] == 'l' and 'single-letter' in row[2] for row in candidates('leva l frid', counts, known))
assert any(row[3] == 'framhovar' for row in candidates('f ramhovar', counts, known))
assert any(row[2] == 'unusual-character' for row in candidates('de"som', counts, known))
assert any('internal-capital' in row[2] for row in candidates('TobiasI', counts, known))
assert not any(row[0] <= 2 for row in candidates('I och i, o och å och ö. A och O. o. s. v. u-farsin.', counts, known))
assert list(candidates('sorn', counts, known))[0][0] == 2
assert list(candidates('sorn', counts, known | {'sorn'}))[0][0] == 3
assert list(candidates('Jerusalem', counts, known))[0][0] == 4
assert any('single-letter' in row[2] for row in candidates('ett a fel', counts, known))
assert any('single-letter' in row[2] for row in candidates('ijååöl!l', counts, known))
print('Swedish audit candidate checks passed')
