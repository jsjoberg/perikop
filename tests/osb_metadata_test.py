"""Validate the shipped OSB metadata and its rebuild from the coordinate frame."""

import json
import hashlib
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
NT = (
    "Matt Mark Luke John Acts Rom 1Cor 2Cor Gal Eph Phil Col 1Thess 2Thess "
    "1Tim 2Tim Titus Philemon Heb James 1Peter 2Peter 1John 2John 3John Jude Rev"
).split()


class MetadataTest(unittest.TestCase):
    def setUp(self):
        self.db = sqlite3.connect(
            f"file:{ROOT/'resources/corpus/osb-reference.db'}?mode=ro", uri=True
        )
        self.addCleanup(self.db.close)

    def test_every_nt_coordinate_matches_the_pinned_kjv_witness(self):
        books = json.loads((ROOT / "resources/corpus/input/KJV.json").read_text())[
            "books"
        ][39:]
        for code, book in zip(NT, books, strict=True):
            expected = {
                (chapter["chapter"], verse["verse"], "")
                for chapter in book["chapters"]
                for verse in chapter["verses"]
            }
            actual = set(
                self.db.execute(
                    "SELECT chapter,verse,suffix FROM coordinate WHERE book=?", (code,)
                )
            )
            self.assertEqual(expected, actual, code)

    def test_database_can_contain_metadata_only(self):
        allowed = {
            "metadata": ["key", "value"],
            "canon": ["position", "book", "name", "abbreviation", "last_chapter"],
            "coordinate": ["id", "book", "chapter", "verse", "suffix", "ordinal"],
            "source": ["id", "code"],
            "source_verse": [
                "id",
                "source",
                "book",
                "chapter",
                "verse",
                "suffix",
                "ordinal",
            ],
            "link": ["osb", "target"],
        }
        tables = {
            name
            for name, in self.db.execute(
                "SELECT name FROM sqlite_master WHERE type='table'"
            )
        }
        self.assertEqual(tables, set(allowed))
        for table, columns in allowed.items():
            self.assertEqual(
                columns, [r[1] for r in self.db.execute(f"PRAGMA table_info({table})")]
            )
        self.assertEqual([], self.db.execute("PRAGMA foreign_key_check").fetchall())
        self.assertEqual(
            [],
            self.db.execute(
                "SELECT id FROM coordinate WHERE chapter<1 OR verse<1 OR length(suffix)>1"
            ).fetchall(),
        )
        self.assertEqual(
            [],
            self.db.execute(
                "SELECT id FROM source_verse WHERE chapter<1 OR verse<1 OR length(book)>12 OR length(suffix)>1"
            ).fetchall(),
        )
        self.assertEqual(
            (36089,), self.db.execute("SELECT count(*) FROM coordinate").fetchone()
        )
        self.assertEqual(
            (78,), self.db.execute("SELECT count(*) FROM canon").fetchone()
        )

    def test_srb_links_use_only_coordinates_from_the_verified_pdf_inventory(self):
        expected = {}
        for line in (
            (ROOT / "resources/content/srb2026-chapters.tsv").read_text().splitlines()
        ):
            if not line or line.startswith("#"):
                continue
            book, chapter, count = line.split("\t")
            expected[book, int(chapter)] = int(count)
        for book, chapter, verse, suffix in self.db.execute(
            "SELECT v.book,v.chapter,v.verse,v.suffix FROM source_verse v JOIN source s ON s.id=v.source WHERE s.code='sv-srb2026'"
        ):
            self.assertEqual(suffix, "")
            self.assertLessEqual(verse, expected[book, chapter])

    def test_frozen_frame_is_coordinate_only_and_preserves_printed_order(self):
        actual = []
        for line in (ROOT / "resources/corpus/osb-frame.tsv").read_text().splitlines():
            if not line or line.startswith("#"):
                continue
            book, chapter, verse, suffix, witness, targets = line.split("\t")
            self.assertRegex(book, r"^[A-Za-z0-9]+$")
            self.assertRegex(suffix, r"^[a-z]?$")
            self.assertIn(witness, ("en-kjv", "grc-lxx"))
            for target in targets.split(";"):
                self.assertRegex(target, r"^(?:[A-Za-z0-9]+ [0-9]+:[0-9]+[a-z]?|-)$")
            actual.append((book, int(chapter), int(verse), suffix))
        expected = self.db.execute(
            "SELECT book,chapter,verse,suffix FROM coordinate ORDER BY ordinal"
        ).fetchall()
        self.assertEqual(expected, actual)

    def test_recorded_public_inputs_are_current(self):
        inputs = json.loads(
            self.db.execute("SELECT value FROM metadata WHERE key='inputs'").fetchone()[
                0
            ]
        )
        for path, expected in inputs.items():
            self.assertEqual(
                expected, hashlib.sha256((ROOT / path).read_bytes()).hexdigest(), path
            )

    def test_rebuild_from_frame(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "osb-reference.db"
            result = subprocess.run(
                [
                    sys.executable,
                    str(ROOT / "tools/corpus/build_osb_reference.py"),
                    "--output",
                    str(output),
                ],
                capture_output=True,
                text=True,
                cwd=ROOT,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            with sqlite3.connect(output) as rebuilt:
                for (table,) in self.db.execute(
                    "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name"
                ):
                    # SQLite's file header varies across versions; compare complete relational data.
                    columns = len(
                        self.db.execute(f"PRAGMA table_info({table})").fetchall()
                    )
                    order = ",".join(str(i) for i in range(1, columns + 1))
                    self.assertEqual(
                        self.db.execute(
                            f"SELECT * FROM {table} ORDER BY {order}"
                        ).fetchall(),
                        rebuilt.execute(
                            f"SELECT * FROM {table} ORDER BY {order}"
                        ).fetchall(),
                        table,
                    )


if __name__ == "__main__":
    unittest.main()
