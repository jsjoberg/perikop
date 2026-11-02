#!/usr/bin/env python3
"""Compare every imported SRB26 verse with an independent Poppler extraction.

Requires pdftohtml on PATH. Full text, locations and differences stay in the local
diagnostics directory. Every remaining difference needs review, including spacing.
"""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import re
import sqlite3
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
PDFS = [("SRBhelaGT.pdf", "f4b3c7f1acb68dd8cb976c1a518bd5338dab8b7d8bda7340e90fdcfaaabd4b70"),
        ("SRBNT-UNnoter.pdf", "59046fcd7f3633bcfd3e46b1778712101a1c820a2714069a757619cbe8b63b75")]
# One-based PDF pages, not printed page numbers. Only coordinates are shared
# with the recipe: this checker does not use its font maps or extraction code.
BOOK_STARTS = [
    "Gen:3 Exod:77 Lev:139 Num:184 Deut:244 Josh:297 Judg:332 Ruth:368 1Sam:373 2Sam:419 "
    "1Kgs:458 2Kgs:505 1Chr:549 2Chr:590 Ezra:639 Neh:653 Esth:674 Job:685 Ps:725 Prov:826 "
    "Eccl:858 Song:870 Isa:876 Jer:949 Lam:1030 Ezek:1038 Dan:1111 Hos:1134 Joel:1146 Amos:1151 "
    "Obad:1160 Jonah:1162 Micah:1165 Nah:1173 Hab:1176 Zeph:1179 Hag:1183 Zech:1186 Mal:1199",
    "Matt:4 Mark:57 Luke:92 John:150 Acts:192 Rom:247 1Cor:270 2Cor:292 Gal:307 Eph:315 "
    "Phil:323 Col:329 1Thess:334 2Thess:339 1Tim:342 2Tim:349 Titus:354 Philemon:357 Heb:359 "
    "James:376 1Peter:382 2Peter:389 1John:393 2John:399 3John:400 Jude:401 Rev:403",
]
CROSS_REFERENCE = re.compile(r"(?:(?:[1-5]\s+)?[A-ZÅÄÖ][a-zåäö]+\.\s+\d+:[\d.,f –-]+\s*)+")


def extract(root, half):
    fonts = {f.attrib["id"]: int(f.attrib["size"]) for f in root.iter("fontspec")}
    starts = [(b, int(p)) for b, p in (s.split(":") for s in BOOK_STARTS[half].split())]
    verses, locations = {}, {}
    key, text, line, last_drop, previous_book = None, "", "", False, None
    line_at, line_spans, line_drop, last_end = None, [], False, 0

    def flush_verse():
        if key:
            assert key not in verses, ("Duplicate verse", key)
            value = " ".join(text.replace("\u00ad", "-").split())
            verses[key] = re.sub(r"\s+([,.;:!?])", r"\1", value)
            assert verses[key], ("Empty verse", key)

    def flush_line():
        nonlocal key, text, line, line_at, line_spans, last_drop, line_drop
        if not line.strip():
            line, line_spans, line_at = "", [], None
            return
        # Poppler may add tracking spaces even inside a printed verse number.
        number = re.match(r"^\s*(\d(?:\s*\d){0,2})\s*(?:\.\s*|(?=[A-ZÅÄÖ]))", line)
        if number and key and int(re.sub(r"\s", "", number[1])) == key[2] + 1:
            flush_verse()
            key, text, line = (key[0], key[1], key[2] + 1), "", line[number.end():]
        if text and not last_drop:
            tail = text.rstrip()
            if tail.endswith(("\u00ad", "-")) and len(tail) > 1 and tail[-2].isalpha():
                text = tail[:-1] if line.lstrip()[:1].islower() else tail
            else:
                text += " "
        text += line
        locations.setdefault(key, []).extend(line_spans)
        last_drop = line_drop
        line, line_at, line_spans, line_drop = "", None, [], False

    for page in root.iter("page"):
        p = int(page.attrib["number"])
        book = next(b for b, begin in reversed(starts) if begin <= p)
        if book != previous_book:
            flush_line()
            flush_verse()
            key = (book, 1, 1) if book in ("Obad", "Philemon", "2John", "3John", "Jude") else None
            text, last_drop, previous_book = "", False, book
        for node in page.findall("text"):
            s = "".join(node.itertext())
            size = fonts[node.attrib["font"]]
            y, x, width, height = (int(node.attrib[a]) for a in ("top", "left", "width", "height"))
            heading = re.fullmatch(r"(?:(?:Kapitel|Psalm) (\d+)|(\d+) Kapitlet)", s.strip())
            if size == 18 and heading:
                flush_line()
                flush_verse()
                key, text, last_drop = (book, int(heading[1] or heading[2]), 1), "", False
                continue
            # Running headers, Latin book labels, and even oversized cross-references.
            if y < 55 or (size == 17 and y < 150) or CROSS_REFERENCE.fullmatch(s.strip()):
                continue
            punctuation = (12 <= size <= 18 and re.fullmatch(r"[.,;:!?\-\u00ad]+", s.strip())
                           and line_at and abs(line_at[1] - y) <= 5 and last_end - 2 <= x <= last_end + 15)
            numeric = (size in (14, 15, 16) and re.fullmatch(r"[0-9.\s]+", s)
                       and any(c.isdigit() for c in s))
            # Accept every font family; sizes at Poppler's fixed 1.5 zoom separate
            # Scripture from summaries, acrostic labels, note markers and footnotes.
            body = size in (15, 16, 17, 40, 41, 42, 43, 44)
            if re.fullmatch(r"[.,;:!?\-\u00ad]+", s.strip()):
                body = False
            if not (body or numeric or punctuation) or not key or not s.strip():
                continue
            # This one full-sized italic summary is printed above the chapter's drop capital.
            if key == ("Lev", 11, 1) and not text and not line and p == 152 and y == 94:
                continue
            if line_at and (line_at[0] != p or
                            (abs(line_at[1] - y) > 2 and not punctuation) or
                            (x < last_end - 10 and not punctuation)):
                flush_line()
            if line and x - last_end > 1.5 and not line[-1].isspace() and not s[0].isspace():
                line += " "
            line += s
            line_at = line_at if punctuation else (p, y)
            last_end, line_drop = x + width, size > 30
            line_spans.append((p, x, y, x + width, height))
        flush_line()
    flush_verse()
    return verses, locations


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pdf_directory", type=Path)
    parser.add_argument("database", type=Path)
    parser.add_argument("diagnostics", type=Path)
    args = parser.parse_args()
    args.diagnostics.mkdir(parents=True, exist_ok=True)
    expected = set()
    chapters = (ROOT / "resources/content/srb2026-chapters.tsv").read_text().splitlines()
    for row in chapters:
        if row and not row.startswith("#"):
            book, chapter, count = row.split("\t")
            expected.update((book, int(chapter), verse) for verse in range(1, int(count) + 1))
    with sqlite3.connect(args.database.resolve().as_uri() + "?mode=ro", uri=True) as db:
        imported = {tuple(row[:3]): row[3] for row in db.execute(
            "SELECT book,chapter,verse,text FROM imported_verse")}
    assert len(expected) == 31171 and imported.keys() == expected, "Imported verse inventory differs"
    printed, locations = {}, {}
    for half, (name, sha) in enumerate(PDFS):
        pdf = args.pdf_directory / name
        assert hashlib.sha256(pdf.read_bytes()).hexdigest() == sha, "Unexpected PDF edition"
        xml = args.diagnostics / ("ot.xml" if half == 0 else "nt.xml")
        subprocess.run(["pdftohtml", "-xml", "-hidden", "-i", "-zoom", "1.5", "-f",
                        "3" if half == 0 else "4", str(pdf.resolve()), str(xml.resolve())],
                       check=True, capture_output=True)
        verses, spans = extract(ET.parse(xml).getroot(), half)
        printed.update(verses)
        locations.update(spans)
    assert printed.keys() == expected, ("Printed verse inventory differs",
                                        expected - printed.keys(), printed.keys() - expected)
    differences = [dict(reference=k, imported=imported[k], poppler=s, locations=locations[k],
                        whitespace_only=re.sub(r"\s", "", s) == re.sub(r"\s", "", imported[k]))
                   for k, s in printed.items() if imported[k] != s]
    counts = collections.Counter(k[0] for k in printed)
    mismatches = collections.Counter(d["reference"][0] for d in differences)
    report = dict(pdf_sha256={name: sha for name, sha in PDFS}, verses=len(printed),
                  chapters=len({k[:2] for k in printed}), exact_matches=len(printed) - len(differences),
                  books={b: dict(verses=n, differences=mismatches[b]) for b, n in counts.items()},
                  differences=differences)
    (args.diagnostics / "comparison.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(f"{len(printed)} verses in {len(counts)} books; {report['exact_matches']} exact matches; "
          f"{len(differences)} differences requiring review")
    for row in differences:
        print(*row["reference"], "spacing" if row["whitespace_only"] else "content")
    return bool(differences)


if __name__ == "__main__":
    raise SystemExit(main())
