"""Compose reviewed SRB numbering choices with the existing public-domain alignment.

Only coordinates are emitted. No SRB or witness text is written to a shipped resource.
"""

import collections, json, sqlite3, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/corpus"))
from mapping import Mapper

SOURCES = ["sv-srb2026", "sv1917", "grc-lxx", "grc-patriarchal", "en-lxx2012", "en-web"]


def rows(verses, books):
    db = sqlite3.connect(":memory:")
    original = sqlite3.connect(ROOT / "resources/corpus/corpus.db")
    original.backup(db)
    original.close()
    db.execute(
        "INSERT INTO source VALUES(6,'en-kjv','en','King James Version','MT','Public-Domain','Numbering witness')"
    )
    ids = dict(db.execute("SELECT code,id FROM book"))
    for code, book in zip(
        books,
        json.loads((ROOT / "resources/corpus/input/KJV.json").read_text())["books"],
    ):
        for chapter in book["chapters"]:
            for verse in chapter["verses"]:
                db.execute(
                    "INSERT INTO verse(source_id,book_id,chapter,verse,last_verse,verse_suffix,text) VALUES(6,?,?,?,?,'',' ')",
                    (ids[code], chapter["chapter"], verse["verse"], verse["verse"]),
                )
    source_ids = dict(db.execute("SELECT code,id FROM source"))

    def coordinate(s):
        import re

        m = re.fullmatch(r"(\d+):(\d+)([a-z]?)", s)
        return (int(m[1]), int(m[2]), m[3]) if m else (None, None, None)

    for line in (ROOT / "resources/corpus/alignment.tsv").read_text().splitlines():
        a, b, book, f0, f1, tbook, t0, t1, kind = line.split("\t")
        if a not in source_ids or b not in source_ids:
            continue
        db.execute(
            "INSERT OR REPLACE INTO alignment VALUES(" + ",".join("?" * 17) + ")",
            (
                source_ids[a],
                source_ids[b],
                ids[book],
                *coordinate(f0),
                *coordinate(f1),
                ids.get(tbook),
                *coordinate(t0),
                *coordinate(t1),
                int(kind),
            ),
        )
    mapper = Mapper(db)
    available = {
        (s, b): set(mapper.coordinates(s, b)) for s in SOURCES[1:] for b in ids
    }
    imported = collections.defaultdict(list)
    for (bi, c, v), _, _ in verses:
        imported[books[bi]].append((c, v, ""))

    def origin(b, c, v):
        if books.index(b) < 39:
            if b in ("Josh", "2Kgs", "Ezra", "Neh") or b == "1Sam" and c in (3, 4):
                return "en-kjv", b, (c, v, "")
            if b == "Ps" and c == 49 and v >= 2:
                return "en-kjv", b, (c, v - 1, "")
            return "sv1917", b, (c, v, "")
        if (
            b == "3John"
            or b == "2Cor"
            and c == 13
            or b == "Rev"
            and (c == 12 and v == 18 or c == 13 and v == 1)
        ):
            return "sv1917", b, (c, v, "")
        return "en-kjv", b, (c, v, "")

    result = []
    for target in SOURCES[1:]:
        forward = {}
        reverse = collections.defaultdict(list)
        for b, refs in imported.items():
            for ref in refs:
                src, ob, ore = origin(b, *ref[:2])
                if src == target:
                    matches = [(ob, ore)]
                else:
                    matches = mapper.counterparts(src, target, ob, ore)
                    if not matches and b not in mapper.new_testament:
                        for gb, gr in mapper.counterparts(src, "grc-lxx", ob, ore):
                            matches += mapper.counterparts("grc-lxx", target, gb, gr)
                matches = sorted(
                    set(
                        (mb, mr)
                        for mb, mr in matches
                        if mr in available.get((target, mb), set())
                    )
                )
                forward[b, ref] = matches
                for point in matches:
                    reverse[point].append((b, ref))

        def emit(a, z, coordinates, edges):
            for b, refs in coordinates.items():
                previous = None
                for ref in refs:
                    to = edges.get((b, ref), [])
                    # Each point maps to a contiguous range in one destination book.
                    if to:
                        assert len({x[0] for x in to}) == 1, (a, z, b, ref, to)
                        tb = to[0][0]
                        t0 = to[0][1]
                        t1 = to[-1][1]
                        kind = 2 if len(to) > 1 else 0 if b == tb and ref == t0 else 1
                    else:
                        tb = ""
                        t0 = t1 = (0, 0, "")
                        kind = 5
                    row = [a, z, kind, b, ref, ref, tb, t0, t1]
                    if previous:
                        # Adjacent source points mapping to one joined destination verse.
                        if len(to) == 1 and previous[6:] == row[6:]:
                            previous[2] = 3
                            previous[5] = ref
                            continue
                        # Same fixed offset, or a run with no counterpart.
                        if (
                            previous[2] == kind
                            and kind in (0, 1, 5)
                            and previous[6] == tb
                            and (
                                kind == 5
                                or (
                                    not ref[2]
                                    and not t0[2]
                                    and not previous[4][2]
                                    and not previous[7][2]
                                    and (ref[0] - t0[0], ref[1] - t0[1])
                                    == (
                                        previous[4][0] - previous[7][0],
                                        previous[4][1] - previous[7][1],
                                    )
                                )
                            )
                        ):
                            previous[5] = ref
                            previous[8] = t1
                            continue
                        result.append(previous)
                    previous = row
                if previous:
                    result.append(previous)

        emit(SOURCES[0], target, imported, forward)
        other = {
            b: mapper.coordinates(target, b)
            for b in ids
            if mapper.coordinates(target, b)
        }
        emit(target, SOURCES[0], other, reverse)
    return result
