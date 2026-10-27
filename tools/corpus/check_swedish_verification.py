#!/usr/bin/env -S uv run --locked
"""Validate the manual scan-review ledger and report complete-chapter coverage.

This checks recorded evidence and detects changes to reviewed text. It cannot
perform or certify the visual review itself. Unlisted chapters are unverified.
"""
import collections
import datetime
import hashlib
import json
import re
import sqlite3
from pathlib import Path
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parents[2]
LEDGER = ROOT / 'docs/reports/sv1917-verification.json'


def text_hash(rows):
    """Hash exact book/chapter/verse/text rows, including punctuation and case."""
    return hashlib.sha256(''.join('%s\t%d\t%d\t%s\n' % row for row in rows).encode('utf-8')).hexdigest()


def validate_evidence(record, key):
    datetime.date.fromisoformat(record['checked_at'])
    if not record['reviewer'].strip() or not record['notes'].strip() or not record['scans']:
        raise ValueError('Missing review evidence: %s' % (key,))
    for scan in record['scans']:
        url = urlparse(scan['url'])
        if (url.scheme != 'https' or url.netloc != 'runeberg.org'
                or not re.fullmatch(r'/img/(?:apokryf/\d{4}|bibeln/efs1927/gt/\d{4})\.1\.tif', url.path)
                or not re.fullmatch(r'[0-9a-f]{64}', scan['sha256'])):
            raise ValueError('Invalid scan witness: %s' % (key,))


def validate(records, chapters):
    seen = set()
    covered = set()
    for record in records:
        key = record['book'], record['chapter']
        if key in seen or key not in chapters:
            raise ValueError('Duplicate or unknown chapter: %s' % (key,))
        seen.add(key)
        rows = chapters[key]
        if record['status'] not in ('verified', 'needs-review'):
            raise ValueError('Unknown review status: %s' % (key,))
        if record['verse_count'] != len(rows) or record['text_sha256'] != text_hash(rows):
            raise ValueError('Reviewed text changed; recheck the scans: %s' % (key,))
        validate_evidence(record, key)
        if record['status'] == 'verified':
            covered.add(key)
    return covered


def validate_unnumbered(records, books, root=ROOT):
    seen, covered = set(), set()
    for record in records:
        key = record['book'], record['title']
        if key in seen or key[0] not in books or not key[1].strip():
            raise ValueError('Duplicate or unknown unnumbered text: %s' % (key,))
        seen.add(key)
        if record['status'] not in ('verified', 'needs-review'):
            raise ValueError('Unknown review status: %s' % (key,))
        path = (root / record['text_path']).resolve()
        if (Path(record['text_path']).is_absolute() or not path.is_relative_to(root.resolve())
                or not path.is_file() or record['text_sha256'] != hashlib.sha256(path.read_bytes()).hexdigest()):
            raise ValueError('Unnumbered text changed or missing: %s' % (key,))
        validate_evidence(record, key)
        if record['status'] == 'verified':
            covered.add(key)
    return covered


def validate_print_witnesses(records, chapters, root=ROOT):
    """Check retained photographs and exact resolved verses, without adding chapter coverage."""
    seen = set()
    for record in records:
        key = record['book'], record['chapter']
        rows = [row for row in chapters.get(key, []) if row[2] == record['verse']]
        path = (root / record['photo_path']).resolve()
        datetime.date.fromisoformat(record['checked_at'])
        decision = record.get('decision', 'corrected')
        correction = record.get('correction_id')
        if (record['id'] in seen or not re.fullmatch(r'VE?\d{3}', record['id'])
                or record['status'] != 'resolved' or not record['reviewer'].strip()
                or not record['source'].strip() or not record['notes'].strip()
                or decision not in ('corrected', 'retained')
                or (decision == 'corrected' and
                    (not isinstance(correction, str) or not re.fullmatch(r'[REA]\d{3}', correction)))
                or (decision == 'retained' and correction is not None)
                or len(rows) != 1 or not record['reading'] or record['reading'] not in rows[0][3]
                or record['text_sha256'] != text_hash(rows)):
            raise ValueError('Invalid or changed print-witness reading: %s' % record['id'])
        if (Path(record['photo_path']).is_absolute() or not path.is_relative_to(root.resolve())
                or path.suffix.lower() not in ('.jpg', '.jpeg', '.png') or not path.is_file()
                or record['photo_sha256'] != hashlib.sha256(path.read_bytes()).hexdigest()):
            raise ValueError('Missing or changed print-witness photograph: %s' % record['id'])
        seen.add(record['id'])
    return seen


def main():
    with sqlite3.connect((ROOT / 'resources/corpus/corpus.db').as_uri() + '?mode=ro', uri=True) as db:
        rows = db.execute("""SELECT b.code,v.chapter,v.verse,v.text,
            CASE WHEN b.deuterocanonical THEN 'AP' ELSE 'GT' END,b.name_sv
            FROM verse v JOIN book b ON b.id=v.book_id JOIN source s ON s.id=v.source_id
            WHERE s.code='sv1917' AND NOT b.new_testament
            ORDER BY b.canonical_order,v.chapter,v.verse""").fetchall()
    chapters = collections.defaultdict(list)
    books = {}
    for book, chapter, verse, text, section, name in rows:
        chapters[book, chapter].append((book, chapter, verse, text))
        books[book] = section, name
    ledger = json.loads(LEDGER.read_text(encoding='utf-8'))
    if ledger['schema_version'] != 1:
        raise ValueError('Unknown verification ledger schema')
    covered = validate(ledger['chapters'], chapters)
    print('book\tsection\tverified_chapters\ttotal_chapters\tverified_verses\ttotal_verses\tstatus')
    for book, (section, name) in books.items():
        keys = {key for key in chapters if key[0] == book}
        done = keys & covered
        count = sum(len(chapters[key]) for key in done)
        total = sum(len(chapters[key]) for key in keys)
        status = 'complete' if done == keys else 'partial' if done else 'unverified'
        print('%s\t%s\t%d\t%d\t%d\t%d\t%s' % (book, section, len(done), len(keys), count, total, status))
    for section in ('AP', 'GT'):
        keys = {key for key in chapters if books[key[0]][0] == section}
        done = keys & covered
        count = sum(len(chapters[key]) for key in done)
        total = sum(len(chapters[key]) for key in keys)
        complete = sum(all(key in covered for key in chapters if key[0] == book)
                       for book in books if books[book][0] == section)
        print('%s: %d/%d chapters; %d/%d verses; %d complete books' % (section, len(done), len(keys), count, total, complete))
    print('%d chapters still need verification' % (len(chapters) - len(covered)))
    unnumbered = ledger.get('unnumbered', [])
    reviewed = validate_unnumbered(unnumbered, books)
    print('Unnumbered source texts: %d/%d verified separately; excluded from chapter/verse counts' %
          (len(reviewed), len(unnumbered)))
    resolved = validate_print_witnesses(ledger.get('resolved_readings', []), chapters)
    print('Resolved readings backed by retained print photographs: %d; no extra chapter coverage' % len(resolved))


if __name__ == '__main__':
    main()
