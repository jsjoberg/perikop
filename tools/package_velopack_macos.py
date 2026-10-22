#!/usr/bin/env python3
"""Pack a staged Perikop.app as a Velopack PKG, portable ZIP, full package, and feed, offline."""

import argparse
from pathlib import Path
import os
import plistlib
import shutil
import subprocess
import tempfile

from check_macos_arch import check

ROOT = Path(__file__).resolve().parents[1]


def pack(bundle, version, architecture, channel, output, vpk=None, delta="None"):
    if architecture != "arm64":
        raise ValueError("macOS packages support Apple Silicon (arm64) only")
    bundle, output = bundle.resolve(), output.resolve()
    vpk = Path(vpk or os.environ.get("PERIKOP_VPK", ROOT / "build/ci/tools/vpk")).resolve()
    runtime = "arm64"
    if channel not in (f"osx-{runtime}-nightly", f"osx-{runtime}-releases"):
        raise ValueError("Channel must match the macOS architecture and track")
    check(bundle, {architecture})
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="perikop-macos-pack-") as temporary:
        work = Path(temporary)
        staged = work / "Perikop.app"
        shutil.copytree(bundle, staged, symlinks=True)
        plist = staged / "Contents/Info.plist"
        metadata = plistlib.loads(plist.read_bytes())
        metadata["CFBundleVersion"] = version
        metadata["CFBundleShortVersionString"] = version.split("-", 1)[0]
        plist.write_bytes(plistlib.dumps(metadata))
        # vpk adds a universal UpdateMac. Give this invocation a thinned copy so every Mach-O in
        # the resulting package has the selected architecture, without changing the restored tools.
        tool = work / "vpk"
        shutil.copytree(vpk / "tools/net8.0/any", tool / "tools/net8.0/any")
        vendor = tool / "vendor"
        vendor.mkdir()
        shutil.copy2(vpk / "vendor/Velopack.entitlements", vendor)
        subprocess.run(["lipo", vpk / "vendor/UpdateMac", "-thin", architecture,
                        "-output", vendor / "UpdateMac"], check=True)
        (vendor / "UpdateMac").chmod(0o755)
        # An ad-hoc signature renews the seal after vpk adds its updater and metadata. No signing
        # identity or notarization service is used; these are the same unsigned packages as today.
        subprocess.run(["dotnet", tool / "tools/net8.0/any/vpk.dll", "pack", "--packId", "Perikop",
                        "--packTitle", "Perikop", "--packVersion", version, "--packDir", staged,
                        "--mainExe", "Perikop", "--runtime", "osx-" + runtime, "--channel", channel,
                        "--delta", delta, "--signAppIdentity", "-", "--outputDir", output], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    parser.add_argument("version")
    parser.add_argument("architecture", choices=("arm64",))
    parser.add_argument("channel")
    parser.add_argument("output", type=Path)
    parser.add_argument("--delta", choices=("None", "BestSpeed", "BestSize"), default="None")
    args = parser.parse_args()
    pack(args.bundle, args.version, args.architecture, args.channel, args.output, delta=args.delta)


if __name__ == "__main__":
    main()
