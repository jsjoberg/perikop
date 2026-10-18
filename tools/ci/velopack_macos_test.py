#!/usr/bin/env python3
"""Prepare a self-contained macOS update validation kit, then run it without networking.

The kit needs only macOS tools on the test machine, with neither Python, .NET, nor a compiler.
Use --prepare-only when transferring it to a macOS 13.4+ machine for the baseline check.
"""

import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from package_velopack_macos import pack


def prepare(bundle, architecture, kit):
    if kit.exists():
        raise ValueError("Validation directory already exists: " + str(kit))
    kit.mkdir(parents=True)
    runtime = {"arm64": "arm64", "x86_64": "x64"}[architecture]
    channel = f"osx-{runtime}-nightly"
    feed = kit / "feed"
    for version in ("1.0.0-test.1", "1.0.0-test.2"):
        pack(bundle, version, architecture, channel, feed)
        if version == "1.0.0-test.1":
            shutil.copy2(feed / f"Perikop-{channel}-Setup.pkg", kit / "first.pkg")
    # Only the installed first version and the local update feed are needed on the test Mac.
    for file in feed.iterdir():
        if file.suffix not in (".nupkg", ".json") or file.name.startswith("assets."):
            file.unlink()
    shutil.copy2(ROOT / "tools/ci/validate_macos_update.command", kit / "validate_macos_update.command")
    (kit / "architecture.txt").write_text(architecture + "\n")
    (kit / "README.txt").write_text(
        "Perikop macOS update validation\n\n"
        "On a Mac with macOS 13.4 or later in the 13 series, open Terminal and run:\n"
        "  bash /path/to/validate_macos_update.command --require-macos13\n\n"
        "On a newer Mac, omit --require-macos13. Intel packages need Intel or Rosetta.\n"
        "No Python, .NET, compiler, network, or administrator access is needed.\n"
        "The test extracts the PKG payload into a temporary directory, checks a real local-feed update,\n"
        "signatures, architecture, UI, and profile preservation, then removes the private copy.\n"
        "It saves result-<architecture>.txt beside this file. On failure it keeps diagnostic files.\n"
        "A pass on a newer macOS does not validate macOS 13 compatibility.\n")
    files = sorted(file for file in kit.rglob("*") if file.is_file())
    with (kit / "SHA256SUMS.txt").open("w") as checksums:
        for file in files:
            with file.open("rb") as stream:
                digest = hashlib.file_digest(stream, "sha256").hexdigest()
            checksums.write(digest + "  " + file.relative_to(kit).as_posix() + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("bundle", type=Path)
    parser.add_argument("architecture", choices=("arm64", "x86_64"))
    parser.add_argument("kit", type=Path)
    parser.add_argument("--prepare-only", action="store_true")
    args = parser.parse_args()
    kit = args.kit.resolve()
    prepare(args.bundle, args.architecture, kit)
    if not args.prepare_only:
        subprocess.run(["bash", kit / "validate_macos_update.command"], check=True)
    subprocess.run(["ditto", "-c", "-k", "--keepParent", kit, str(kit) + ".zip"], check=True)
    print("Validation kit: " + str(kit) + ".zip")


if __name__ == "__main__":
    main()
