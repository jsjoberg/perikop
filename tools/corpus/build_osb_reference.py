#!/usr/bin/env python3
"""Build text-free OSB references and crosswalks from a private local EPUB.

OSB wording is used only for local comparison with pinned public-domain witnesses.
Diagnostics containing words must remain under build/, never resources/.
"""

import argparse
from collections import Counter, defaultdict
import hashlib
import json
import math
from pathlib import Path
import re
import sqlite3
import sys
import zipfile

from audit_osb import NT
from osb_epub import read
from align_editions import words, load, names
from align_rules import CANON
from mapping import Mapper

ROOT = Path(__file__).resolve().parents[2]
SOURCES = ["sv1917", "grc-lxx", "grc-patriarchal", "en-lxx2012", "en-web"]
OSB_SHA256 = "3c15e77c4496a60507b3b5fa5fef72ad9d58de2a4cfeb2848000ee7ddf8470d4"
FRAME = ROOT / "resources/corpus/osb-frame.tsv"


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
        "tools/corpus/osb_epub.py",
        "tools/corpus/audit_osb.py",
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
    # Freeze the reviewed factual bridge as text so future rebuilds need no private EPUB.
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
                    "Orthodox Study Bible, Thomas Nelson 2008, ISBN 9781418578633",
                ),
                ("epub_sha256", OSB_SHA256),
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
        "epub_sha256": OSB_SHA256,
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


def align_english(source, target):
    """Lexical sequence alignment; retain confidence for every non-empty link."""
    if not target:
        return {}, []
    replacements = {
        "saith": "says",
        "spake": "spoke",
        "begat": "begot",
        "shew": "show",
        "sheweth": "shows",
        "shewed": "showed",
        "henceforth": "now",
        "judgement": "judgment",
        "an": "a",
        "amongst": "among",
    }

    def tokens(text):
        return {replacements.get(w, w) for w in words(text)} | {
            "@" + name for name in names(text)
        }

    variants_s = [[tokens(v) for v in t.split("\x1e")] for _, t in source]
    variants_t = [[tokens(v) for v in t.split("\x1e")] for _, t in target]
    ss = [set().union(*variants) for variants in variants_s]
    tt = [set().union(*variants) for variants in variants_t]
    frequency = Counter(w for sentence in ss + tt for w in sentence)
    idf = {
        w: max(0.15, math.log((len(ss) + len(tt)) / count))
        * (0.7 if w.startswith("@") else 1)
        for w, count in frequency.items()
    }

    def groups(sentences, variants):
        result = {}
        for end in range(1, len(sentences) + 1):
            for size in range(1, min(3, end) + 1):
                options = []
                for index in range(max(len(v) for v in variants)):
                    group = set().union(
                        *(v[min(index, len(v) - 1)] for v in variants[end - size : end])
                    )
                    options.append((group, sum(idf[w] for w in group)))
                result[end, size] = options
        return result

    gs, gt = groups(ss, variants_s), groups(tt, variants_t)
    n, m = len(ss), len(tt)
    band = max(80, abs(n - m) + 30)
    paths = {}
    scores = {0: {0: 0.0}}
    beads = [(1, 1), (1, 2), (2, 1), (1, 3), (3, 1), (1, 0), (0, 1)]

    def similarity(i, di, j, dj):
        return max(
            2 * sum(idf[w] for w in s & t) / (sw + tw or 1)
            for s, sw in gs[i, di]
            for t, tw in gt[j, dj]
        )

    for i in range(n + 1):
        row = scores.setdefault(i, {})
        centre = i * m / max(n, 1)
        for j in range(max(0, int(centre - band)), min(m, int(centre + band)) + 1):
            if not i and not j:
                continue
            best = (math.inf, None)
            for di, dj in beads:
                old = scores.get(i - di, {}).get(j - dj)
                if old is None:
                    continue
                cost = (
                    2.5
                    if not di or not dj
                    else 4 * (1 - similarity(i, di, j, dj)) + 1.15 * (di + dj - 2)
                )
                if di == dj == 1 and source[i - 1][0][1:] == target[j - 1][0][1:]:
                    cost -= 0.15
                candidate = (old + cost, (di, dj))
                if candidate[0] < best[0]:
                    best = candidate
            if best[1] is not None:
                row[j] = best[0]
                paths[i, j] = best[1]
        scores.pop(i - 3, None)
    if (n, m) not in paths:
        raise ValueError("OSB alignment band exhausted")
    links = defaultdict(set)
    review = []
    i, j = n, m
    while i or j:
        di, dj = paths[i, j]
        score = similarity(i, di, j, dj) if di and dj else 0
        sr = [r for r, _ in source[i - di : i]]
        tr = [r for r, _ in target[j - dj : j]]
        # Low-confidence wording is kept in diagnostics, never guessed at runtime.
        if score >= 0.20:
            for r in sr:
                links[r].update(tr)
        if score < 0.4 or di != 1 or dj != 1 or (sr and tr and sr[0][1:] != tr[0][1:]):
            review.append((sr, tr, round(score, 3)))
        i -= di
        j -= dj
    return links, list(reversed(review))


def generate(epub, diagnostics, output, reuse=False, from_metadata=False):
    if (
        not from_metadata
        and hashlib.sha256(epub.read_bytes()).hexdigest() != OSB_SHA256
    ):
        raise ValueError("Unexpected OSB reference edition")
    osb = read(epub) if not from_metadata else {}
    db = sqlite3.connect(":memory:")
    base = sqlite3.connect(ROOT / "resources/corpus/corpus.db")
    base.backup(db)
    base.close()
    witnesses(db)
    if from_metadata:
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
        return
    if reuse:
        cache = diagnostics / "comparison-inputs.json"
        if not cache.exists() or json.loads(cache.read_text()) != input_hashes():
            raise ValueError(
                "Comparison inputs changed; run a full comparison instead of reusing diagnostics"
            )
        matches = defaultdict(dict)
        for *ref, targets in json.loads(
            (diagnostics / "brenton-links.json").read_text()
        ):
            matches[ref[0]][tuple(ref)] = set(map(tuple, targets))
        export_reference(db, osb, matches, output)
        return
    mapper = Mapper(db)
    text_cache = {
        (s, b, c, v, suffix): t
        for s, b, c, v, suffix, t in db.execute(
            "SELECT source.code,book.code,chapter,verse,verse_suffix,text FROM verse JOIN source ON source.id=source_id JOIN book ON book.id=book_id"
        )
    }

    def enrich(ref, text):
        variants = [text]
        for witness in ("en-kjv", "en-lxx2012"):
            counterparts = mapper.counterparts("grc-lxx", witness, ref[0], ref[1:])
            wording = " ".join(
                text_cache.get((witness, b, *r), "") for b, r in counterparts
            ).strip()
            variants.append(wording or text)
        return ref, "\x1e".join(variants)

    bybook = defaultdict(list)
    for ref, text in osb.items():
        bybook[ref[0]].append((ref, text))
    matches = {}
    reviews = {}
    for book, verses in bybook.items():
        if book in NT:
            continue
        other = "EsthGr" if book == "Esth" else "Ezra" if book == "Neh" else book
        target = load(db, "en-brenton", other)
        target = [
            (r, t) for r, t in target if r[1:] in mapper.coordinates("grc-lxx", r[0])
        ]
        if book == "Sir":
            # OSB retains the Latin chapter order in 30–36; Brenton follows the Greek order.
            def sir_order(row):
                _, c, v, s = row[0]
                if c == 1 and v == 1 and s < "h":
                    return (-1, 0, 0)
                if c < 30 or c == 30 and v <= 24:
                    return (0, c, v)
                if c == 33 and v == 12:
                    return (1, 0, 0)
                if c in (34, 35):
                    return (2, c, v)
                if c == 36 and v <= 16:
                    return (3, 0, v)
                if c == 30:
                    return (4, 0, v)
                if c in (31, 32):
                    return (5, c, v)
                if c == 33:
                    return (6, 0, v)
                if c == 36:
                    return (7, 0, v)
                return (8, c, v)

            target = [row for row in target if sir_order(row)[0] >= 0]
            target.sort(key=sir_order)
        if book == "Prov":
            # OSB gives Agur and Lemuel their own chapters 30 and 31.
            def prov_order(row):
                _, c, v, s = row[0]
                if c < 24 or c == 24 and (v < 22 or v == 22 and s <= "e"):
                    return (0, c, v, s)
                if c == 24 and 23 <= v <= 34:
                    return (1, c, v, s)
                if 25 <= c <= 29:
                    return (2, c, v, s)
                if c == 24 and v == 22:
                    return (3, c, v, s)
                if c == 24 and 35 <= v <= 53:
                    return (4, c, v, s)
                if c == 24 and v >= 54:
                    return (5, c, v, s)
                return (6, c, v, s)

            target.sort(key=prov_order)
        if book in ("Ezra", "Neh"):
            target = [(r, t) for r, t in target if (r[1] <= 10) == (book == "Ezra")]
        if book == "2Chr":
            target += load(db, "en-brenton", "PrMan")
        target = [enrich(r, t) for r, t in target]
        matches[book], reviews[book] = align_english(verses, target)
        missing = [(r, t) for r, t in verses if not matches[book].get(r)]
        print(book, len(verses), "Brenton unlinked", len(missing), flush=True)
    diagnostics.mkdir(parents=True, exist_ok=True)
    (diagnostics / "unlinked.json").write_text(
        json.dumps(
            [
                [*r, t]
                for b, verses in bybook.items()
                if b not in NT
                for r, t in verses
                if not matches[b].get(r)
            ],
            ensure_ascii=False,
            indent=1,
        )
    )
    (diagnostics / "brenton-links.json").write_text(
        json.dumps(
            [
                [*r, sorted(targets)]
                for b in matches
                for r, targets in matches[b].items()
            ],
            ensure_ascii=False,
        )
    )
    (diagnostics / "review.json").write_text(
        json.dumps(reviews, ensure_ascii=False, indent=1)
    )
    (diagnostics / "osb-local.json").write_text(
        json.dumps([[*r, t] for r, t in osb.items()], ensure_ascii=False)
    )
    db.commit()
    disk = sqlite3.connect(diagnostics / "witnesses.db")
    db.backup(disk)
    disk.close()
    export_reference(db, osb, matches, output)
    (diagnostics / "comparison-inputs.json").write_text(
        json.dumps(input_hashes(), sort_keys=True)
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("epub", type=Path, nargs="?")
    parser.add_argument("--diagnostics", type=Path, default=ROOT / "build/osb")
    parser.add_argument(
        "--output", type=Path, default=ROOT / "resources/corpus/osb-reference.db"
    )
    parser.add_argument("--reuse-comparison", action="store_true")
    parser.add_argument(
        "--from-metadata",
        action="store_true",
        help="Rebuild from the shipped factual frame without an OSB EPUB",
    )
    args = parser.parse_args()
    if (args.epub is None) != args.from_metadata:
        parser.error("Provide either a local EPUB or --from-metadata")
    generate(
        args.epub,
        args.diagnostics,
        args.output,
        args.reuse_comparison,
        args.from_metadata,
    )
