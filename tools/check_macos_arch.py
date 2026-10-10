#!/usr/bin/env python3
"""Require Intel and Apple Silicon slices in every Mach-O file in an app bundle."""

import argparse
from pathlib import Path
import subprocess

MACHO_MAGICS = {bytes.fromhex(value) for value in (
    "feedface", "cefaedfe", "feedfacf", "cffaedfe", "cafebabe", "bebafeca", "cafebabf", "bfbafeca")}
ARCHITECTURES = {"arm64", "x86_64"}


def check(bundle):
    count = 0
    errors = []
    for path in sorted(bundle.rglob("*")):
        if not path.is_file() or path.is_symlink():
            continue
        with path.open("rb") as stream:
            if stream.read(4) not in MACHO_MAGICS:
                continue
        count += 1
        architectures = set(subprocess.check_output(["/usr/bin/lipo", "-archs", str(path)], text=True).split())
        if architectures != ARCHITECTURES:
            errors.append("{}: expected arm64 and x86_64, found {}".format(path, " ".join(sorted(architectures))))
    if not count:
        errors.append("No Mach-O files found in " + str(bundle))
    if errors:
        raise ValueError("\n".join(errors))
    print("Checked {} Mach-O files: arm64 and x86_64 in every binary".format(count))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    args = parser.parse_args()
    try:
        check(args.bundle)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + "\n")


if __name__ == "__main__":
    main()
