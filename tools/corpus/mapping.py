"""Passage mapping between editions, as CorpusDb::counterparts and map_passage do at run time.

build_corpus.py uses it to state lectionary references in shipped numbering and to carry
paragraph boundaries across editions. Keep it in step with src/storage/corpus_db.cpp.
"""


class Mapper:
    def __init__(self, db):
        self.db = db
        self.versification = dict(db.execute('SELECT code,versification FROM source'))
        self.new_testament = {code for code, in db.execute('SELECT code FROM book WHERE new_testament=1')}
        self.cache = {}

    def coordinates(self, source, book):
        if (source, book) not in self.cache:
            self.cache[(source, book)] = [tuple(r) for r in self.db.execute(
                'SELECT chapter,verse,verse_suffix FROM verse JOIN source ON source.id=source_id JOIN book ON '
                'book.id=book_id WHERE source.code=? AND book.code=? ORDER BY chapter,verse,verse_suffix',
                (source, book))]
        return self.cache[(source, book)]

    def alignment(self, a, b, book, first, last):
        row = self.db.execute(
            'SELECT kind,from_first_chapter,from_first_verse,from_first_suffix,from_last_chapter,from_last_verse,'
            "from_last_suffix,coalesce(target.code,''),to_first_chapter,to_first_verse,to_first_suffix,"
            'to_last_chapter,to_last_verse,to_last_suffix FROM alignment JOIN source f ON f.id=from_source '
            'JOIN source t ON t.id=to_source JOIN book ON book.id=book_id LEFT JOIN book target ON '
            'target.id=to_book_id WHERE f.code=? AND t.code=? AND book.code=? AND '
            '(from_first_chapter,from_first_verse,from_first_suffix)<=(?,?,?) AND '
            '(from_last_chapter,from_last_verse,from_last_suffix)>=(?,?,?) '
            'ORDER BY from_first_chapter DESC,from_first_verse DESC,from_first_suffix DESC LIMIT 1',
            (a, b, book, *first, *last)).fetchone()
        if not row:
            return None
        return row[0], tuple(row[1:4]), tuple(row[4:7]), row[7], tuple(row[8:11]), tuple(row[11:14])

    def same_numbering(self, a, b, book):
        return a in self.versification and b in self.versification and (
            self.versification[a] == self.versification[b] or book in self.new_testament)

    def counterparts(self, a, b, book, ref):
        found = self.alignment(a, b, book, ref, ref)
        if not found:
            return [(book, ref)] if self.same_numbering(a, b, book) else []
        kind, f0, f1, target, t0, t1 = found
        if not target:
            return []
        if f0 == f1 or kind == 3:
            return [(target, r) for r in self.coordinates(b, target) if t0 <= r <= t1]
        return [(target, (ref[0] + t0[0] - f0[0], ref[1] + t0[1] - f0[1], ''))]

    def map_passage(self, a, b, book, first, last):
        """[(book, first, last)] in b's numbering for a passage in a's numbering."""
        if a == b:
            return [(book, first, last)]
        found = []
        for ref in self.coordinates(a, book):
            if first <= ref <= last:
                for target in self.counterparts(a, b, book, ref):
                    if target not in found:
                        found.append(target)
        # Verses that the target only reorders within one run read as one range in its own order.
        if found and all(t[0] == found[0][0] for t in found):
            refs = self.coordinates(b, found[0][0])
            at = [refs.index(t[1]) if t[1] in refs else len(refs) for t in found]
            if at != sorted(at) and len(refs) not in at:
                run = all(i in at or not self.counterparts(b, a, found[0][0], refs[i])
                          for i in range(min(at), max(at) + 1))
                if run:
                    found.sort(key=lambda t: refs.index(t[1]))
        # Join targets that are adjacent in the target edition, including verses between
        # them that only the target tradition has.
        result = []
        for target_book, ref in found:
            refs = self.coordinates(b, target_book)
            if ref not in refs:
                continue
            at = refs.index(ref)
            if result and result[-1][0] == target_book:
                previous = refs.index(result[-1][2]) if result[-1][2] in refs[:at] else at
                if previous != at and all(not self.counterparts(b, a, target_book, between)
                                          for between in refs[previous + 1:at]):
                    result[-1] = (target_book, result[-1][1], ref)
                    continue
            result.append((target_book, ref, ref))
        return result
