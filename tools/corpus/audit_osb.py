#!/usr/bin/env python3
"""Audit a local OSB EPUB's numeric verse anchors; never export its Bible text.

This is a development audit, not a complete OSB coordinate importer. In particular,
unanchored lettered additions in Esther need a separate reviewed extraction.
"""

import argparse
import collections
import hashlib
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET
import zipfile


ROOT = Path(__file__).resolve().parents[2]
ALIASES = {
    "C1hr": "1Chr",
    "C2hr": "2Chr",
    "K1gdms": "1Sam",
    "K2gdms": "2Sam",
    "K3gdms": "1Kgs",
    "K4gdms": "2Kgs",
    "E1sd": "1Esd",
    "M1acc": "1Macc",
    "M2acc": "2Macc",
    "M3acc": "3Macc",
    "Bar": "Baruch",
    "Mic": "Micah",
    "obad": "Obad",
    "zech": "Zech",
    "C1or": "1Cor",
    "C2or": "2Cor",
    "J1ohn": "1John",
    "J2ohn": "2John",
    "J3ohn": "3John",
    "T1hess": "1Thess",
    "T2hess": "2Thess",
    "T1im": "1Tim",
    "T2im": "2Tim",
    "P1et": "1Peter",
    "P2et": "2Peter",
    "Phlm": "Philemon",
    "Jas": "James",
}
NT = (
    "Matt Mark Luke John Acts Rom 1Cor 2Cor Gal Eph Phil Col 1Thess 2Thess "
    "1Tim 2Tim Titus Philemon Heb James 1Peter 2Peter 1John 2John 3John Jude Rev"
).split()
ANCHOR = re.compile(r"([^_]+)_vchap(\d+)-(\d+)")


def audit(epub):
    coordinates = collections.defaultdict(set)
    with zipfile.ZipFile(epub) as archive:
        for name in archive.namelist():
            if not name.endswith((".html", ".xhtml")):
                continue
            for element in ET.fromstring(archive.read(name)).iter():
                match = ANCHOR.fullmatch(element.get("id", ""))
                if not match:
                    continue
                prefix, chapter, verse = match.groups()
                book = ALIASES.get(prefix, prefix)
                ref = (int(chapter), int(verse))
                if ref in coordinates[book]:
                    raise ValueError(
                        f"Duplicate numeric anchor: {book} {chapter}:{verse}"
                    )
                coordinates[book].add(ref)

    witness = json.loads((ROOT / "resources/corpus/input/KJV.json").read_text())[
        "books"
    ][39:]
    differences = {}
    for book, kjv in zip(NT, witness, strict=True):
        expected = {
            (chapter["chapter"], verse["verse"])
            for chapter in kjv["chapters"]
            for verse in chapter["verses"]
        }
        if coordinates[book] != expected:
            differences[book] = {
                "osb_only": sorted(coordinates[book] - expected),
                "kjv_only": sorted(expected - coordinates[book]),
            }

    chapters = {}
    for book, refs in sorted(coordinates.items()):
        by_chapter = collections.defaultdict(list)
        for chapter, verse in sorted(refs):
            by_chapter[chapter].append(verse)
        chapters[book] = dict(by_chapter)
    return {
        "input_sha256": hashlib.sha256(epub.read_bytes()).hexdigest(),
        "scope": "Numeric anchors only; excludes unanchored lettered additions. Not a semantic crosswalk.",
        "nt_books": len(NT),
        "nt_coordinates": sum(len(coordinates[book]) for book in NT),
        "nt_kjv_coordinate_differences": differences,
        "numeric_verse_anchors": chapters,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("epub", type=Path, help="Local user-supplied OSB EPUB")
    parser.add_argument(
        "--output", type=Path, required=True, help="Local metadata-only JSON report"
    )
    args = parser.parse_args()
    report = audit(args.epub)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(
        f"{report['nt_books']} NT books; {report['nt_coordinates']} numeric coordinates"
    )
    if report["nt_kjv_coordinate_differences"]:
        raise SystemExit(
            "OSB and KJV numeric coordinate inventories differ; inspect the report"
        )
    print(
        "OSB and pinned KJV NT coordinate inventories match; content alignment remains a separate check"
    )


if __name__ == "__main__":
    main()
