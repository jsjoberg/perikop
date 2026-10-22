#!/usr/bin/env python3
"""Give a nightly its version, and write status notes for its successful platforms."""

import argparse
from pathlib import Path
import re

# Velopack channel platforms, in the order the notes list them.
PLATFORMS = {
    "linux-x64": "Linux x64",
    "linux-arm64": "Linux ARM64",
    "osx-arm64": "macOS Apple Silicon",
    "win-x64": "Windows x64",
}


def version(base, released, day, run_number, run_attempt):
    """A SemVer version above every earlier nightly and release.

    Before the base version is released, nightlies are its prereleases: 0.2.0-nightly.20261018.412.1.
    After it, they are prereleases of the next patch, so updates never offer them as older than the release.
    """
    if not re.fullmatch(r"\d+\.\d+\.\d+", base):
        raise ValueError("Invalid project version: " + base)
    if not re.fullmatch(r"\d{4}-\d{2}-\d{2}", day):
        raise ValueError("Invalid nightly date: " + day)
    major, minor, patch = (int(part) for part in base.split("."))
    if released:
        patch += 1
    return f"{major}.{minor}.{patch}-nightly.{day.replace('-', '')}.{int(run_number)}.{int(run_attempt)}"


def prepare(updates, notes_path, repository, commit, run_id, tag, sanitizers, build_version=None):
    """Write the notes for the platforms whose feeds are in updates; None when there are none."""
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("The nightly needs a full commit SHA")
    if not re.fullmatch(r"nightly-\d{4}-\d{2}-\d{2}-" + commit[:7] + r"(?:-\d+(?:-\d+)?)?", tag):
        raise ValueError("The nightly tag does not match the commit")
    available = set()
    for feed in sorted(updates.glob("releases.*.json")):
        platform = feed.name[len("releases."):-len("-nightly.json")]
        if not feed.name.endswith("-nightly.json") or platform not in PLATFORMS:
            raise ValueError("Unknown nightly feed: " + feed.name)
        available.add(platform)
    if not available:
        return None

    missing = [name for platform, name in PLATFORMS.items() if platform not in available]
    complete = not missing and sanitizers == "success"
    status = "complete" if complete else "partial"
    notes = [f"<!-- perikop-nightly: {status} -->", f"Nightly build of commit `{commit}`.", ""]
    if build_version:
        notes += [f"Version `{build_version}`.", ""]
    if not complete:
        notes += ["**Incomplete nightly.** Only the available platform packages appear in this release.", ""]
    notes += ["Available platforms:"] + ["- " + name for platform, name in PLATFORMS.items() if platform in available]
    if missing:
        notes += ["", "Unavailable platforms:"] + ["- " + name for name in missing]
    sanitizer_status = "passed" if sanitizers == "success" else sanitizers
    notes += ["", f"Sanitizers: **{sanitizer_status}**.", "",
              f"See [the build](https://github.com/{repository}/actions/runs/{run_id}), "
              f"[the changes](https://github.com/{repository}/blob/{commit}/CHANGELOG.md), and "
              f"[installation](https://github.com/{repository}/blob/{commit}/docs/using-perikop.md#installation)."]
    notes_path.write_text("\n".join(notes) + "\n")
    return tag


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    versioning = commands.add_parser("version")
    versioning.add_argument("--base", required=True)
    versioning.add_argument("--released", action="store_true")
    versioning.add_argument("--date", required=True)
    versioning.add_argument("--run-number", type=int, required=True)
    versioning.add_argument("--run-attempt", type=int, required=True)
    preparing = commands.add_parser("prepare")
    preparing.add_argument("--updates", type=Path, required=True)
    preparing.add_argument("--notes", type=Path, required=True)
    preparing.add_argument("--repository", required=True)
    preparing.add_argument("--commit", required=True)
    preparing.add_argument("--run-id", required=True)
    preparing.add_argument("--tag", required=True)
    preparing.add_argument("--sanitizers", choices=("success", "failure", "cancelled", "skipped"), required=True)
    preparing.add_argument("--version", default=None)
    args = parser.parse_args()
    try:
        if args.command == "version":
            print(version(args.base, args.released, args.date, args.run_number, args.run_attempt))
        else:
            tag = prepare(args.updates, args.notes, args.repository, args.commit, args.run_id, args.tag,
                          args.sanitizers, args.version)
            if tag:
                print(tag)
    except ValueError as error:
        parser.exit(1, str(error) + "\n")
