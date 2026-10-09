#!/usr/bin/env python3
"""Reject ELF dependencies newer than the packaged app's glibc baseline."""

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys

BASELINE = (2, 34)


def glibc_requirements(output):
    # Definitions in libc are not dependencies. Only inspect version needs.
    needs = False
    versions = set()
    for line in output.splitlines():
        if line.startswith("Version "):
            needs = line.startswith("Version needs section")
        if needs:
            versions.update(re.findall(r"Name: (GLIBC_[A-Za-z0-9_.]+)", line))
    return versions


def check(paths, readelf="readelf"):
    count = 0
    maximum = (0, 0)
    errors = []
    files = []
    for path in paths:
        files.extend(sorted(path.rglob("*")) if path.is_dir() else [path])
    for path in files:
        if not path.is_file() or path.is_symlink():
            continue
        with path.open("rb") as stream:
            if stream.read(4) != b"\x7fELF":
                continue
        count += 1
        output = subprocess.check_output(
            [readelf, "--version-info", str(path)], universal_newlines=True,
            env=dict(os.environ, LC_ALL="C"))
        for name in sorted(glibc_requirements(output)):
            suffix = name[len("GLIBC_"):]
            if not re.fullmatch(r"\d+(?:\.\d+)+", suffix):
                errors.append("{}: unsupported requirement {}".format(path, name))
                continue
            version = tuple(int(part) for part in suffix.split("."))
            maximum = max(maximum, version)
            if version > BASELINE:
                errors.append("{}: requires {} (maximum GLIBC_2.34)".format(path, name))
    if not count:
        errors.append("No ELF files found")
    if errors:
        raise ValueError("\n".join(errors))
    print("Checked {} ELF files: maximum GLIBC_{} (limit 2.34)".format(
        count, ".".join(str(part) for part in maximum)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", type=Path, nargs="+")
    parser.add_argument("--readelf", default="readelf")
    args = parser.parse_args()
    try:
        check(args.paths, args.readelf)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + "\n")


if __name__ == "__main__":
    main()
