#!/usr/bin/env -S uv run --locked
"""Coverage must not survive changed text or accept duplicate/missing evidence."""
import copy
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/corpus'))
from check_swedish_verification import text_hash, validate

rows = [('PrMan', 1, 1, 'Herre, du Allsmäktige.')]
chapters = {('PrMan', 1): rows}
record = {'book': 'PrMan', 'chapter': 1, 'status': 'verified', 'verse_count': 1,
          'text_sha256': text_hash(rows), 'checked_at': '2026-10-25',
          'reviewer': 'Test reviewer', 'notes': 'Full visual check.',
          'scans': [{'url': 'https://runeberg.org/img/apokryf/0265.1.tif', 'sha256': 'a' * 64}]}
assert validate([record], chapters) == {('PrMan', 1)}
pending = record | {'status': 'needs-review'}
assert validate([pending], chapters) == set()
assert validate([], chapters) == set()


def rejects(records, data=chapters):
    try:
        validate(records, data)
    except ValueError:
        return
    raise AssertionError('Invalid or stale coverage was accepted')


rejects([record, record])
rejects([record], {('PrMan', 1): [('PrMan', 1, 1, 'Herre, du Allsmäktige!')]})
rejects([record | {'verse_count': 2}])
rejects([record | {'scans': []}])
rejects([record | {'status': 'OCR-agrees'}])
rejects([record | {'book': 'Unknown'}])
bad = copy.deepcopy(record)
bad['scans'][0]['url'] = 'https://example.com/scan.tif'
rejects([bad])
print('Swedish verification ledger checks passed')
