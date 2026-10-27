#!/usr/bin/env -S uv run --locked
"""Coverage must not survive changed text or accept duplicate/missing evidence."""
import copy
import hashlib
import tempfile
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/corpus'))
from check_swedish_verification import text_hash, validate, validate_unnumbered, validate_print_witnesses

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
with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    path = root / 'foretal.txt'
    path.write_text('Genom lagen och profeterna.\n')
    item = record | {'book': 'Sir', 'title': 'Företal', 'text_path': 'foretal.txt',
                     'text_sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
    assert validate_unnumbered([item], {'Sir'}, root) == {('Sir', 'Företal')}
    assert validate_unnumbered([item | {'status': 'needs-review'}], {'Sir'}, root) == set()
    for invalid in ([item, item], [item | {'book': 'Unknown'}],
                    [item | {'text_path': '../outside.txt'}], [item | {'scans': []}]):
        try:
            validate_unnumbered(invalid, {'Sir'}, root)
        except ValueError:
            pass
        else:
            raise AssertionError('Invalid unnumbered evidence was accepted')
    path.write_text('Changed text.\n')
    try:
        validate_unnumbered([item], {'Sir'}, root)
    except ValueError:
        pass
    else:
        raise AssertionError('Changed unnumbered text was accepted')
with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    photo = root / 'witness.jpg'
    photo.write_bytes(b'original photograph')
    witness = {'id': 'V001', 'correction_id': 'R023', 'book': 'PrMan', 'chapter': 1, 'verse': 1,
               'status': 'resolved', 'reading': 'Herre', 'text_sha256': text_hash(rows),
               'checked_at': '2026-10-25', 'reviewer': 'Human and visual check',
               'source': 'Another print, supplied by the user', 'notes': 'Confirmed reading.',
               'photo_path': 'witness.jpg', 'photo_sha256': hashlib.sha256(photo.read_bytes()).hexdigest()}
    assert validate_print_witnesses([witness], chapters, root) == {'V001'}
    assert validate_print_witnesses([], chapters, root) == set()
    retained = witness | {'id': 'VE001', 'decision': 'retained', 'correction_id': None}
    assert validate_print_witnesses([retained], chapters, root) == {'VE001'}
    for invalid in ([witness, witness], [witness | {'reading': 'Different'}],
                    [witness | {'verse': 2}], [witness | {'photo_path': '../outside.jpg'}],
                    [witness | {'photo_path': 'missing.jpg'}], [witness | {'photo_sha256': 'a' * 64}],
                    [witness | {'text_sha256': 'a' * 64}], [witness | {'reviewer': ''}]):
        try:
            validate_print_witnesses(invalid, chapters, root)
        except ValueError:
            pass
        else:
            raise AssertionError('Invalid print evidence or stale resolved verse was accepted')
    photo.write_bytes(b'changed photograph')
    try:
        validate_print_witnesses([witness], chapters, root)
    except ValueError:
        pass
    else:
        raise AssertionError('Changed photograph was accepted')
    assert validate([], chapters) == set(), 'A resolved verse must not imply a whole chapter review'
print('Swedish verification ledger checks passed')
