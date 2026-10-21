#!/usr/bin/env -S uv run --locked
"""Generate the frozen v2 reading-plan ranges from Swedish text lengths.

Use --check to verify the committed data, or --write to regenerate it. A change
to published ranges requires a new plan version and progress migration.
"""

import argparse
from collections import Counter
import math
from pathlib import Path
import sqlite3
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/corpus"))
from mapping import Mapper

OUTPUT = ROOT / "src/core/reading_plan_parts.inc"
PLANS = [
    ("nt", "Nya testamentet", "", "Matt Mark Luke John Acts Rom 1Cor 2Cor Gal Eph Phil Col "
     "1Thess 2Thess 1Tim 2Tim Titus Philemon Heb James 1Peter 2Peter 1John 2John 3John Jude Rev"),
    ("ot", "Gamla testamentet", "Den hebreiska bibelns böcker.",
     "Gen Exod Lev Num Deut Josh Judg Ruth 1Sam 2Sam 1Kgs 2Kgs 1Chr 2Chr Ezra Neh Esth Job Ps "
     "Prov Eccl Song Isa Jer Lam Ezek Dan Hos Joel Amos Obad Jonah Micah Nah Hab Zeph Hag Zech Mal"),
    ("lxx", "Septuagintas övriga böcker", "Böckerna utanför den hebreiska bibeln.",
     "PrMan Tob Jdt Wis Sir Baruch EpJer Sus Bel 1Macc 2Macc"),
]
SEPARATE = {"Matt", "Mark", "Luke", "John", "Acts", "Rev"}
LETTERS = set(PLANS[0][3].split()) - SEPARATE


def chapter_words(db, books, testament):
    """Count each Swedish counterpart once, using the reader's LXX framing."""
    mapper = Mapper(db)
    texts = {(book, c, v, suffix): text for book, c, v, suffix, text in db.execute(
        "SELECT book.code,chapter,verse,verse_suffix,text FROM verse "
        "JOIN source ON source.id=source_id JOIN book ON book.id=book_id WHERE source.code='sv1917'")}
    canon = {book: (frame, first, last) for book, frame, first, last in db.execute(
        "SELECT book,frame_book,first_chapter,last_chapter FROM canon")}
    rows = []
    for book in books.split():
        frame, first, last = canon[book]
        source = "sv1917" if testament == "nt" else "grc-lxx"
        counts = Counter()
        seen = set()
        for ref in mapper.coordinates(source, frame):
            if ref[0] < first or (last and ref[0] > last) or (book == "Ps" and ref[0] > 150):
                continue
            chapter = ref[0] - first + 1
            counts.setdefault(chapter, 0)
            targets = [(frame, ref)] if source == "sv1917" else mapper.counterparts(source, "sv1917", frame, ref)
            for target_book, target in targets:
                identity = (target_book, *target)
                if identity not in seen:
                    counts[chapter] += len(texts.get(identity, "").split())
                    seen.add(identity)
        if not counts or not sum(counts.values()):
            raise ValueError(f"No Swedish text for {book}")
        rows.extend((book, chapter, counts[chapter]) for chapter in sorted(counts))
    return rows


def partition(rows, target, count=None):
    """Balance chapter ranges; only complete short books may share a part.

    A book that fits within 1.6 target lengths stays whole. Larger books split
    at chapter boundaries. An extra cost for cuts inside letters favors fewer,
    slightly longer letter parts. The NT has exactly 60 parts; other plans
    choose their own count around the same target.
    """
    n = len(rows)
    prefix, starts, ends, totals = [0], {}, {}, Counter()
    for i, (book, _, words) in enumerate(rows):
        prefix.append(prefix[-1] + words)
        starts.setdefault(book, i)
        ends[book] = i + 1
        totals[book] += words
    candidates = [[] for _ in range(n + 1)]
    for end in range(1, n + 1):
        for begin in range(end - 1, -1, -1):
            words = prefix[end] - prefix[begin]
            if words > 1.6 * target and end - begin > 1:
                break
            first, last = rows[begin][0], rows[end - 1][0]
            if first != last:
                if begin != starts[first] or end != ends[last]:
                    continue
                if any(rows[i][0] in SEPARATE for i in range(begin, end)):
                    continue
            elif totals[first] <= 1.6 * target and (begin != starts[first] or end != ends[first]):
                continue
            cut_cost = 0 if end == ends[last] else 0.35 if last in LETTERS else 0.05
            candidates[end].append((begin, ((words - target) / target) ** 2 + cut_cost + 0.05))
    previous = [math.inf] * (n + 1)
    previous[0] = 0
    paths = {}
    if count:
        for k in range(1, count + 1):
            current = [math.inf] * (n + 1)
            for end in range(1, n + 1):
                for begin, cost in candidates[end]:
                    if previous[begin] + cost < current[end]:
                        current[end] = previous[begin] + cost
                        paths[k, end] = begin
            previous = current
        if not math.isfinite(previous[n]):
            raise ValueError(f"Cannot split the plan into {count} parts")
        end, bounds = n, []
        for k in range(count, 0, -1):
            begin = paths[k, end]
            bounds.append((begin, end))
            end = begin
    else:
        for end in range(1, n + 1):
            previous[end], paths[end] = min(
                ((previous[begin] + cost, begin) for begin, cost in candidates[end]),
                default=(math.inf, -1))
        if not math.isfinite(previous[n]):
            raise ValueError("Cannot split the plan")
        end, bounds = n, []
        while end:
            begin = paths[end]
            bounds.append((begin, end))
            end = begin
    return list(reversed(bounds))


def generate():
    with sqlite3.connect(f"file:{ROOT / 'resources/corpus/corpus.db'}?mode=ro", uri=True) as db:
        chapters = [chapter_words(db, books, ident) for ident, _, _, books in PLANS]
    target = sum(row[2] for row in chapters[0]) / 60
    lines = ["// Generated by tools/build_reading_plans.py; frozen plan version 2.",
             "// Published range changes require a new version and migration."]
    for (ident, title, description, _), rows in zip(PLANS, chapters):
        bounds = partition(rows, target, 60 if ident == "nt" else None)
        lines.extend([f'{{"{ident}", "{title}", "{description}", {{'])
        weights = []
        for begin, end in bounds:
            ranges = []
            for book, chapter, _ in rows[begin:end]:
                if ranges and ranges[-1][0] == book:
                    ranges[-1][2] = chapter
                else:
                    ranges.append([book, chapter, chapter])
            lines.append("    {" + ", ".join(f'{{"{book}", {first}, {last}}}' for book, first, last in ranges) + "},")
            weights.append(sum(row[2] for row in rows[begin:end]))
        lines.append("}},")
        print(f"{ident}: {len(bounds)} parts; {min(weights)}–{max(weights)} words, "
              f"mean {sum(weights) / len(weights):.0f}")
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    data = generate()
    if arguments.write:
        OUTPUT.write_text(data, encoding="utf-8")
    elif OUTPUT.read_text(encoding="utf-8") != data:
        raise SystemExit("Reading-plan data differs; review versioning before regenerating it")
