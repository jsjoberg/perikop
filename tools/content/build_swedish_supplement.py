#!/usr/bin/env python3
"""Resolve additional Swedish forms against the existing independent dictionary inputs.

The local diagnostics JSON is only a vocabulary query; no verses, quotations or
PDF-derived definitions are written to the supplement. All definitions and morphology
come from the pinned Dalin, Nyström and SALDO sources under their existing licenses.
"""
import argparse, collections, hashlib, json, re, sqlite3, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/lexicon"))
from build_study import build

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument("local_verses", type=Path, nargs="?")
ap.add_argument(
    "--pronunciations-only",
    action="store_true",
    help="Rebuild only the pronunciation table, which needs no local verses",
)
args = ap.parse_args()
if args.pronunciations_only == bool(args.local_verses):
    ap.error("Give the local verses, or --pronunciations-only")
source = sqlite3.connect(ROOT / "resources/corpus/corpus.db")
out = ROOT / "resources/content/swedish-study.db"


def write_pronunciations():
    # Variant names reuse the established pronunciation rather than deriving speech from PDF typography.
    # One explicit transaction, so a bad alias leaves the old table in place.
    dest = sqlite3.connect(out, isolation_level=None)
    try:
        dest.execute("BEGIN")
        dest.execute("DROP TABLE IF EXISTS pronunciation")
        dest.execute(
            "CREATE TABLE pronunciation(language TEXT,source TEXT,spoken TEXT,phonemes TEXT,priority INTEGER,PRIMARY KEY(language,source)) STRICT, WITHOUT ROWID"
        )
        for line in (
            (ROOT / "resources/content/pronunciation-aliases.tsv").read_text(encoding="utf-8").splitlines()
        ):
            if not line or line.startswith("#"):
                continue
            form, target = line.split("\t")
            for suffix in ("", "s"):
                known = source.execute(
                    "SELECT spoken,phonemes,priority FROM pronunciation WHERE language='sv' AND source=?",
                    (target + suffix,),
                ).fetchone()
                spoken, phonemes, priority = known if known else (target + suffix, "", 100)
                dest.execute(
                    "INSERT INTO pronunciation VALUES(?,?,?,?,?)",
                    ("sv", form + suffix, spoken, phonemes, priority),
                )
        dest.execute("COMMIT")
        dest.execute("VACUUM")
    except BaseException:
        if dest.in_transaction:
            dest.execute("ROLLBACK")
        raise
    finally:
        dest.close()


def record_hash():
    manifest_path = ROOT / "resources/manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    for asset in manifest["assets"]:
        if asset["path"] == "resources/content/swedish-study.db":
            asset["sha256"] = hashlib.sha256(out.read_bytes()).hexdigest()
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n"
    )


if args.pronunciations_only:
    write_pronunciations()
    record_hash()
    sys.exit()
words = collections.Counter(
    w
    for *_, text in json.loads(args.local_verses.read_text(encoding="utf-8"))
    for w in re.findall(r"[^\W\d_]+", text.lower())
)
db = sqlite3.connect(ROOT / "build/srb/study-work.db")
source.backup(db)
for table in ("sv_word", "sv_biblical", "sv_saldo", "dalin", "biblical", "saldo"):
    db.execute("DELETE FROM " + table)
build(db, words)
db.commit()
out.unlink(missing_ok=True)
dest = sqlite3.connect(out)
schema = (ROOT / "resources/corpus/schema.sql").read_text(encoding="utf-8")
for table in ("dalin", "sv_word", "biblical", "sv_biblical", "saldo", "sv_saldo"):
    statement = re.search(r"CREATE TABLE " + table + r"\s*\(.*?;", schema, re.S)[0]
    dest.execute(statement)
for forms, entries, column in [
    ("sv_word", "dalin", "dalin"),
    ("sv_biblical", "biblical", "article"),
    ("sv_saldo", "saldo", "entry"),
]:
    known = {f for f, in source.execute("SELECT DISTINCT form FROM " + forms)}
    rows = [row for row in db.execute("SELECT * FROM " + forms) if row[0] not in known]
    used = {row[1] for row in rows}
    values = [row for row in db.execute("SELECT * FROM " + entries) if row[0] in used]
    if values:
        dest.executemany(
            "INSERT INTO "
            + entries
            + " VALUES("
            + ",".join("?" * len(values[0]))
            + ")",
            values,
        )
    if rows:
        dest.executemany(
            "INSERT INTO " + forms + " VALUES(" + ",".join("?" * len(rows[0])) + ")",
            rows,
        )
    print(forms, len(rows), "entries", len(values))
dest.execute("PRAGMA user_version=1")
dest.execute("PRAGMA application_id=1330795603")
dest.commit()
dest.execute("VACUUM")
dest.close()
write_pronunciations()
record_hash()
