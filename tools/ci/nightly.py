#!/usr/bin/env python3
"""Prepare successful platform packages and explicit status notes for a nightly."""

import argparse
import hashlib
from pathlib import Path
import re

PLATFORMS = {
    "x86_64.AppImage": "Linux x64",
    "aarch64.AppImage": "Linux ARM64",
    "macOS-universal.dmg": "macOS universal",
    "windows-x64.exe": "Windows x64",
}


def prepare(packages, repository, commit, run_id, tag, sanitizers):
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("The nightly needs a full commit SHA")
    if not re.fullmatch(r"nightly-\d{4}-\d{2}-\d{2}-" + commit[:7] + r"(?:-\d+(?:-\d+)?)?", tag):
        raise ValueError("The nightly tag does not match the commit")
    files = sorted(packages.glob("Perikop-*"))
    if not files:
        return None
    selected = {}
    versions = set()
    for file in files:
        suffix = next((suffix for suffix in PLATFORMS if file.name.endswith("-" + suffix)), None)
        if suffix is None or not file.is_file():
            raise ValueError("Unknown nightly package: " + file.name)
        version = file.name[len("Perikop-"):-len("-" + suffix)]
        if not re.fullmatch(r"\d+\.\d+\.\d+", version):
            raise ValueError("Invalid nightly package version: " + file.name)
        if suffix in selected:
            raise ValueError("Duplicate nightly platform: " + PLATFORMS[suffix])
        versions.add(version)
        selected[suffix] = file
    if len(versions) != 1:
        raise ValueError("The nightly packages have different versions")

    missing = [name for suffix, name in PLATFORMS.items() if suffix not in selected]
    complete = not missing and sanitizers == "success"
    status = "complete" if complete else "partial"
    notes = [f"<!-- perikop-nightly: {status} -->", f"Nightly build of commit `{commit}`.", ""]
    if not complete:
        notes += ["**Incomplete nightly.** Only the available platform packages appear in this release.", ""]
    notes += ["Available platforms:"]
    checksums = []
    for suffix, file in selected.items():
        renamed = packages / ("Perikop-" + tag + "-" + suffix)
        file.rename(renamed)
        with renamed.open("rb") as stream:
            checksum = hashlib.file_digest(stream, "sha256").hexdigest()
        checksums.append(f"{checksum}  {renamed.name}\n")
        notes.append(f"- {PLATFORMS[suffix]}")
    if missing:
        notes += ["", "Unavailable platforms:"] + ["- " + name for name in missing]
    sanitizer_status = "passed" if sanitizers == "success" else sanitizers
    notes += ["", f"Sanitizers: **{sanitizer_status}**.", "",
              f"See [the build](https://github.com/{repository}/actions/runs/{run_id}), "
              f"[the changes](https://github.com/{repository}/blob/{commit}/CHANGELOG.md), and "
              f"[installation](https://github.com/{repository}/blob/{commit}/docs/using-perikop.md#installation)."]
    (packages / "SHA256SUMS.txt").write_text("".join(sorted(checksums, key=lambda line: line.split("  ", 1)[1])))
    (packages / "NOTES.md").write_text("\n".join(notes) + "\n")
    return tag


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--packages", type=Path, required=True)
    parser.add_argument("--repository", required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--sanitizers", choices=("success", "failure", "cancelled", "skipped"), required=True)
    args = parser.parse_args()
    try:
        tag = prepare(args.packages, args.repository, args.commit, args.run_id, args.tag, args.sanitizers)
    except ValueError as error:
        parser.exit(1, str(error) + "\n")
    if tag:
        print(tag)
