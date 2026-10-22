#!/usr/bin/env python3
"""Build text-free OSB references and crosswalks from the frozen coordinate frame.

osb-frame.tsv records OSB's printed verse order and each coordinate's numbering
witness; osb-reviewed.tsv holds the reviewed Greek counterparts. Neither has Bible wording.
"""

import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import re
import sqlite3
import sys
import zipfile

from align_editions import load
from align_rules import CANON
from mapping import Mapper

ROOT = Path(__file__).resolve().parents[2]
SOURCES = ["sv1917", "grc-lxx", "grc-patriarchal", "en-lxx2012", "en-web"]
FRAME = ROOT / "resources/corpus/osb-frame.tsv"
NT = (
    "Matt Mark Luke John Acts Rom 1Cor 2Cor Gal Eph Phil Col 1Thess 2Thess "
    "1Tim 2Tim Titus Philemon Heb James 1Peter 2Peter 1John 2John 3John Jude Rev"
).split()


def input_hashes():
    paths = [
        "resources/corpus/corpus.db",
        "resources/corpus/alignment.tsv",
        "resources/corpus/books.tsv",
        "resources/corpus/input/KJV.json",
        "resources/corpus/input/eng-Brenton_usfm.zip",
        "resources/corpus/osb-reviewed.tsv",
        "resources/content/srb2026-chapters.tsv",
        "tools/corpus/build_osb_reference.py",
        "tools/corpus/align_editions.py",
        "tools/corpus/align_rules.py",
        "tools/corpus/mapping.py",
        "tools/content/srb_alignment.py",
    ]
    result = {
        path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in paths
    }
    if FRAME.exists():
        result[FRAME.relative_to(ROOT).as_posix()] = hashlib.sha256(
            FRAME.read_bytes()
        ).hexdigest()
    return result


def export_reference(db, osb, matches, output):
    """Store coordinates and translation links only, without any witness wording."""
    # These three books retain their verse divisions, apart from two joined endings.
    # Do not let archaic/modern English vocabulary invent split or shifted verses.
    for book, extra in [
        ("1Macc", {(8, 32, "")}),
        ("2Macc", set()),
        ("3Macc", {(1, 29, "")}),
    ]:
        printed = {r[1:] for r in osb if r[0] == book}
        greek = set(r[0][1:] for r in load(db, "grc-lxx", book))
        if greek - printed != extra or printed - greek:
            raise ValueError(("Maccabees inventory changed", book))
        matches[book] = {r: {r} for r in osb if r[0] == book}
    matches["1Macc"][("1Macc", 8, 31, "")].add(("1Macc", 8, 32, ""))
    matches["3Macc"][("3Macc", 1, 28, "")].add(("3Macc", 1, 29, ""))

    def parse(value):
        m = re.fullmatch(r"(\w+) (\d+):(\d+)([a-z]?)", value)
        if not m:
            raise ValueError(("Invalid reviewed OSB reference", value))
        return m[1], int(m[2]), int(m[3]), m[4]

    for line in (ROOT / "resources/corpus/osb-reviewed.tsv").read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        left, right = line.split("\t")
        ref = parse(left)
        targets = set(map(parse, right.split(";")))
        if ref not in osb:
            raise ValueError(("Reviewed coordinate absent from OSB", ref))
        for target in targets:
            if target[1:] not in Mapper(db).coordinates("grc-lxx", target[0]):
                raise ValueError(("Reviewed Greek coordinate absent", target))
        matches[ref[0]][ref] = targets
    # Write the reviewed counterparts back into the frame, so it stays the one complete record.
    lines = [
        "# OSB printed order and reviewed witness coordinates. No Bible wording.",
        "# book\tchapter\tverse\tsuffix\twitness\tcounterparts",
    ]
    for ref in osb:
        nt = ref[0] in NT
        targets = [ref] if nt else sorted(matches.get(ref[0], {}).get(ref, set()))

        def formatted(target):
            return f"{target[0]} {target[1]}:{target[2]}{target[3]}"

        lines.append(
            "\t".join(
                [
                    ref[0],
                    str(ref[1]),
                    str(ref[2]),
                    ref[3],
                    "en-kjv" if nt else "grc-lxx",
                    ";".join(map(formatted, targets)) or "-",
                ]
            )
        )
    frozen = "\n".join(lines) + "\n"
    if output.resolve() == (ROOT / "resources/corpus/osb-reference.db").resolve():
        FRAME.write_text(frozen)
    elif not FRAME.exists() or FRAME.read_text() != frozen:
        raise ValueError(
            "The reviewed frame changed; generate and review the primary metadata first"
        )
    # The optional SRB edition has a structural inventory independent of the private PDFs.
    sys.path.insert(0, str(ROOT / "tools/content"))
    from srb_alignment import rows as srb_rows

    ids = dict(db.execute("SELECT code,id FROM book"))
    db.execute(
        "INSERT INTO source VALUES(8,'sv-srb2026','sv','SRB2026','SRB2026',NULL,'Local import')"
    )
    srb = []
    for line in (
        (ROOT / "resources/content/srb2026-chapters.tsv").read_text().splitlines()
    ):
        if not line or line.startswith("#"):
            continue
        b, c, n = line.split("\t")
        for v in range(1, int(n) + 1):
            srb.append(((CANON.index(b), int(c), v), [], ""))
            db.execute(
                "INSERT INTO verse(source_id,book_id,chapter,verse,last_verse,verse_suffix,text) VALUES(8,?,?,?,?,'',' ')",
                (ids[b], int(c), v, v),
            )
    source_ids = dict(db.execute("SELECT code,id FROM source"))
    for a, z, kind, b, f0, f1, tb, t0, t1 in srb_rows(srb, CANON):
        absent = (None, None, None)
        db.execute(
            "INSERT OR REPLACE INTO alignment VALUES(" + ",".join("?" * 17) + ")",
            (
                source_ids[a],
                source_ids[z],
                ids[b],
                *f0,
                *f1,
                ids.get(tb),
                *(t0 if tb else absent),
                *(t1 if tb else absent),
                kind,
            ),
        )
    mapper = Mapper(db)
    available = {
        (s, b): set(mapper.coordinates(s, b))
        for s in SOURCES + ["sv-srb2026"]
        for b in ids
    }
    bridges = {r: targets for table in matches.values() for r, targets in table.items()}
    forward = defaultdict(dict)
    for ref in osb:
        b, c, v, suffix = ref
        if b in NT:
            greek = [
                (gb, gr)
                for gb, gr in mapper.counterparts(
                    "en-kjv", "grc-patriarchal", b, (c, v, suffix)
                )
                if gr in available.get(("grc-patriarchal", gb), set())
            ]
        else:
            greek = [
                (r[0], r[1:])
                for r in bridges.get(ref, [])
                if r[1:] in available.get(("grc-lxx", r[0]), set())
            ]
        frame = "grc-patriarchal" if b in NT else "grc-lxx"
        for target in SOURCES + ["sv-srb2026"]:
            found = []
            for gb, gr in greek:
                found += (
                    [(gb, gr)]
                    if target == frame
                    else mapper.counterparts(frame, target, gb, gr)
                )
            # An English witness may preserve a verse absent from the Greek source.
            # Only NT numbering has been independently verified against the complete KJV inventory.
            if b in NT and not greek and target in ("sv1917", "en-web", "sv-srb2026"):
                found = mapper.counterparts("en-kjv", target, b, (c, v, suffix))
            forward[target][ref] = set(
                (tb, *tr)
                for tb, tr in found
                if tr in available.get((target, tb), set())
            )

    order = {r: i for i, r in enumerate(osb)}
    # An indivisible source verse must be displayed/read once for its whole OSB unit.
    # Connected components preserve many-to-many mappings instead of dropping a target.
    for source, table in forward.items():
        parent = {r: r for r in osb}

        def root(r):
            while parent[r] != r:
                parent[r] = parent[parent[r]]
                r = parent[r]
            return r

        def join(a, b):
            a, b = root(a), root(b)
            if a != b:
                parent[b] = a

        inverse = defaultdict(list)
        for ref, targets in table.items():
            for target in targets:
                inverse[target].append(ref)
        for refs in inverse.values():
            for previous, ref in zip(refs, refs[1:]):
                # A repeated passage elsewhere in OSB is a separate occurrence, not one huge merge.
                if ref[0] == previous[0] and order[ref] == order[previous] + 1:
                    join(previous, ref)
        grouped = defaultdict(set)
        for ref, targets in table.items():
            grouped[root(ref)].update(targets)
        for ref in table:
            table[ref] = grouped[root(ref)]

    fingerprints = input_hashes()
    output.parent.mkdir(parents=True, exist_ok=True)
    temp = output.with_suffix(".tmp")
    temp.unlink(missing_ok=True)
    with sqlite3.connect(temp) as out:
        out.executescript(
            """
            PRAGMA foreign_keys=ON;
            CREATE TABLE metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL) STRICT;
            CREATE TABLE canon(position INTEGER PRIMARY KEY,book TEXT UNIQUE NOT NULL,name TEXT NOT NULL,abbreviation TEXT NOT NULL,last_chapter INTEGER NOT NULL) STRICT;
            CREATE TABLE coordinate(id INTEGER PRIMARY KEY,book TEXT NOT NULL,chapter INTEGER NOT NULL,verse INTEGER NOT NULL,suffix TEXT NOT NULL,ordinal INTEGER UNIQUE NOT NULL,UNIQUE(book,chapter,verse,suffix)) STRICT;
            CREATE TABLE source(id INTEGER PRIMARY KEY,code TEXT UNIQUE NOT NULL) STRICT;
            CREATE TABLE source_verse(id INTEGER PRIMARY KEY,source INTEGER NOT NULL REFERENCES source(id),book TEXT NOT NULL,chapter INTEGER NOT NULL,verse INTEGER NOT NULL,suffix TEXT NOT NULL,ordinal INTEGER NOT NULL,UNIQUE(source,book,chapter,verse,suffix)) STRICT;
            CREATE TABLE link(osb INTEGER NOT NULL REFERENCES coordinate(id),target INTEGER NOT NULL REFERENCES source_verse(id),PRIMARY KEY(osb,target)) WITHOUT ROWID,STRICT;
            CREATE INDEX reverse_link ON link(target,osb);
        """
        )
        out.executemany(
            "INSERT INTO metadata VALUES(?,?)",
            [
                (
                    "authority",
                    "Orthodox Study Bible, Thomas Nelson 2008",
                ),
                ("schema", "osb-v1"),
                ("inputs", json.dumps(fingerprints, sort_keys=True)),
            ],
        )
        for ref, index in order.items():
            out.execute(
                "INSERT INTO coordinate VALUES(?,?,?,?,?,?)", (index + 1, *ref, index)
            )
        books = list(dict.fromkeys(r[0] for r in osb))
        for index, book in enumerate(books):
            name, abbr = db.execute(
                "SELECT name_sv,abbreviation_sv FROM book WHERE code=?", (book,)
            ).fetchone()
            out.execute(
                "INSERT INTO canon VALUES(?,?,?,?,?)",
                (index, book, name, abbr, max(r[1] for r in osb if r[0] == book)),
            )
        next_id = 1
        for index, (source, table) in enumerate(forward.items(), 1):
            out.execute("INSERT INTO source VALUES(?,?)", (index, source))
            targets = sorted(set().union(*table.values()))
            target_ids = {r: next_id + i for i, r in enumerate(targets)}
            next_id += len(targets)
            for target, identifier in target_ids.items():
                out.execute(
                    "INSERT INTO source_verse VALUES(?,?,?,?,?,?,?)",
                    (identifier, index, *target, identifier),
                )
            for ref, targets in table.items():
                out.executemany(
                    "INSERT INTO link VALUES(?,?)",
                    [(order[ref] + 1, target_ids[t]) for t in sorted(targets)],
                )
            print(
                "OSB crosswalk",
                source,
                sum(bool(t) for t in table.values()),
                "of",
                len(osb),
                flush=True,
            )
        out.execute("PRAGMA application_id=1330795602")
        out.execute("PRAGMA user_version=1")
        assert not out.execute("PRAGMA foreign_key_check").fetchall()
        out.commit()
        out.execute("VACUUM")
    temp.replace(output)
    provenance = {
        "authority": "osb-v1",
        "coordinates": len(osb),
        "picker_entries": len(books),
        "nt_coordinates": sum(r[0] in NT for r in osb),
        "reviewed_points": sum(
            bool(line) and not line.startswith("#")
            for line in (ROOT / "resources/corpus/osb-reviewed.tsv")
            .read_text()
            .splitlines()
        ),
        "inputs": fingerprints,
        "source_coverage": {
            source: sum(bool(targets) for targets in table.values())
            for source, table in forward.items()
        },
    }
    report = output.with_name("osb-provenance.json")
    report.write_text(json.dumps(provenance, ensure_ascii=False, indent=2) + "\n")
    if output.resolve() == (ROOT / "resources/corpus/osb-reference.db").resolve():
        manifest_path = ROOT / "resources/manifest.json"
        manifest = json.loads(manifest_path.read_text())
        for path in [output, report, FRAME, ROOT / "resources/corpus/osb-reviewed.tsv"]:
            relative = path.relative_to(ROOT).as_posix()
            asset = next((a for a in manifest["assets"] if a["path"] == relative), None)
            if asset is None:
                asset = {
                    "path": relative,
                    "license": "MIT",
                    "role": "OSB coordinate metadata and source crosswalks; no Bible wording",
                }
                manifest["assets"].append(asset)
            asset["sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
        manifest_path.write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2) + "\n"
        )


def witnesses(db):
    ids = dict(db.execute("SELECT code,id FROM book"))
    db.execute(
        "INSERT INTO source VALUES(6,'en-kjv','en','KJV','MT','Public-Domain','Numbering witness')"
    )
    for code, book in zip(
        CANON,
        json.loads((ROOT / "resources/corpus/input/KJV.json").read_text())["books"],
    ):
        for chapter in book["chapters"]:
            for verse in chapter["verses"]:
                db.execute(
                    "INSERT INTO verse(source_id,book_id,chapter,verse,last_verse,verse_suffix,text) VALUES(?,?,?,?,?,'',?)",
                    (
                        6,
                        ids[code],
                        chapter["chapter"],
                        verse["verse"],
                        verse["verse"],
                        verse["text"],
                    ),
                )
    db.execute(
        "INSERT INTO source VALUES(7,'en-brenton','en','Brenton','LXX','Public-Domain','Wording witness')"
    )
    usfm = {
        row[1]: row[0]
        for line in (ROOT / "resources/corpus/books.tsv").read_text().splitlines()
        if line and not line.startswith("#")
        for row in [line.split("\t")]
    }
    with zipfile.ZipFile(
        ROOT / "resources/corpus/input/eng-Brenton_usfm.zip"
    ) as archive:
        for name in archive.namelist():
            if not name.endswith(".usfm"):
                continue
            text = archive.read(name).decode("utf-8-sig")
            code = re.search(r"\\id (\w+)", text)[1]
            book = {"DAG": "Dan", "NEH": None}.get(code, usfm.get(code))
            if not book:
                continue
            text = re.sub(r"\\(?:f|x|fe)\s.*?\\(?:f|x|fe)\*", "", text, flags=re.S)
            text = re.sub(r"\\\+?w\s+([^|\\]+)(?:\|[^\\]*)?\\\+?w\*", r"\1", text)
            chunks = re.split(r"\\c\s+(\d+)", text)
            for i in range(1, len(chunks), 2):
                chapter = int(chunks[i])
                body = chunks[i + 1]
                verses = list(re.finditer(r"\\v\s+(\d+)([a-z]?)(?:-(\d+))?\s+", body))
                for j, match in enumerate(verses):
                    chunk = body[
                        match.end() : (
                            verses[j + 1].start() if j + 1 < len(verses) else len(body)
                        )
                    ]
                    chunk = re.sub(r"\\(?:s\d*|ms\d*|r|d|sp|cl|cp)\s+[^\n]*", "", chunk)
                    chunk = re.sub(r"\\[+a-zA-Z0-9]+\*?\s*", " ", chunk)
                    plain = " ".join(chunk.split())
                    if plain:
                        db.execute(
                            "INSERT INTO verse(source_id,book_id,chapter,verse,last_verse,verse_suffix,text) VALUES(?,?,?,?,?,?,?)",
                            (
                                7,
                                ids[book],
                                chapter,
                                int(match[1]),
                                int(match[3] or match[1]),
                                match[2],
                                plain,
                            ),
                        )
    # Runtime omits the witnesses, but the pinned full table includes their mappings.
    sources = dict(db.execute("SELECT code,id FROM source"))

    def ref(value):
        m = re.fullmatch(r"(\d+):(\d+)([a-z]?)", value)
        return (int(m[1]), int(m[2]), m[3]) if m else (None, None, None)

    for line in (ROOT / "resources/corpus/alignment.tsv").read_text().splitlines():
        a, b, book, f0, f1, tbook, t0, t1, kind = line.split("\t")
        db.execute(
            "INSERT OR REPLACE INTO alignment VALUES(" + ",".join("?" * 17) + ")",
            (
                sources[a],
                sources[b],
                ids[book],
                *ref(f0),
                *ref(f1),
                ids.get(tbook),
                *ref(t0),
                *ref(t1),
                int(kind),
            ),
        )


def generate(output):
    db = sqlite3.connect(":memory:")
    base = sqlite3.connect(ROOT / "resources/corpus/corpus.db")
    base.backup(db)
    base.close()
    witnesses(db)
    osb = {}
    matches = defaultdict(dict)
    for line in FRAME.read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        b, c, v, s, witness, targets = line.split("\t")
        ref = (b, int(c), int(v), s)
        if ref in osb:
            raise ValueError(("Duplicate frozen coordinate", ref))
        osb[ref] = ""
        if b in NT:
            if witness != "en-kjv" or targets != f"{b} {c}:{v}{s}":
                raise ValueError("Invalid NT witness")
        else:
            if witness != "grc-lxx":
                raise ValueError("Invalid OT witness")
            parsed = set()
            for value in targets.split(";") if targets != "-" else []:
                m = re.fullmatch(r"(\w+) (\d+):(\d+)([a-z]?)", value)
                if not m:
                    raise ValueError(("Invalid frozen witness reference", value))
                parsed.add((m[1], int(m[2]), int(m[3]), m[4]))
            matches[b][ref] = parsed
    export_reference(db, osb, matches, output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=ROOT / "resources/corpus/osb-reference.db"
    )
    generate(parser.parse_args().output)
