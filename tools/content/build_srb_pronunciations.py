#!/usr/bin/env python3
"""Fill SRB26 pronunciation gaps from the pinned CC0 NST dictionary, without verse text.

Existing pronunciations win. Only exact lowercase forms with a single convertible
NST pronunciation are added; ambiguous or unknown forms keep the normal fallback.
"""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import re
import sqlite3
import sys
import tarfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/lexicon"))
from fetch import fetch
from phonemes import to_espeak, to_ipa


def nst_pronunciations(lines, wanted):
    found = collections.defaultdict(set)
    unsupported = set()
    for raw in lines:
        fields = raw.decode("latin-1").rstrip().split(";")
        if len(fields) <= 11 or fields[0] not in wanted:
            continue
        ipa = to_ipa(fields[11])
        if not ipa:
            unsupported.add(fields[0])
            continue
        try:
            found[fields[0]].add(to_espeak(ipa))
        except ValueError:
            unsupported.add(fields[0])
    return {word: next(iter(phones)) for word, phones in found.items() if len(phones) == 1 and word not in unsupported}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("local_verses", type=Path)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    words = collections.Counter(
        word.lower()
        for *_, text in json.loads(args.local_verses.read_text(encoding="utf-8"))
        for word in re.findall(r"[a-zA-ZåäöÅÄÖéèüáàâëï]+", text)
    )
    known = set()
    for name in ("lexicon.tsv", "g2p-corpus.tsv"):
        known.update(
            line.split("\t")[0]
            for line in (ROOT / "resources/speech" / name).read_text(encoding="utf-8").splitlines()
        )
    for name in ("corpus/corpus.db", "content/swedish-study.db"):
        with sqlite3.connect(f"file:{ROOT / 'resources' / name}?mode=ro", uri=True) as db:
            known.update(word.lower() for word, in db.execute("SELECT source FROM pronunciation WHERE language='sv'"))
    missing = {word: count for word, count in words.items() if word not in known}
    with tarfile.open(fetch("nst")) as archive:
        member = next(item for item in archive.getmembers() if item.isfile() and item.name.endswith(".pron"))
        added = nst_pronunciations(archive.extractfile(member), missing)
    output = ROOT / "resources/speech/nst-srb2026.tsv"
    output.write_text(
        "".join(f"{word}\t{added[word]}\n" for word in sorted(added, key=lambda word: word.encode("utf-8"))),
        encoding="utf-8", newline="\n",
    )
    report = {
        "word_forms": len(words),
        "word_occurrences": words.total(),
        "missing_before": len(missing),
        "missing_occurrences_before": sum(missing.values()),
        "added_forms": len(added),
        "missing_after": len(missing) - len(added),
        "missing_occurrences_after": sum(count for word, count in missing.items() if word not in added),
        "remaining": sorted(((word, count) for word, count in missing.items() if word not in added), key=lambda item: (-item[1], item[0])),
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    manifest_path = ROOT / "resources/manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["assets"] = [asset for asset in manifest["assets"] if asset.get("path") != "resources/speech/nst-srb2026.tsv"]
    manifest["assets"].append({
        "path": "resources/speech/nst-srb2026.tsv",
        "sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
        "url": json.loads((ROOT / "tools/lexicon/inputs.json").read_text())["nst"]["url"],
        "license": "CC0-1.0",
        "generated": "tools/content/build_srb_pronunciations.py",
        "role": "Unambiguous NST pronunciations for additional SRB26 vocabulary; existing speech data and corrections retain priority",
    })
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: value for key, value in report.items() if key != "remaining"}, indent=2))


if __name__ == "__main__":
    main()
