#!/usr/bin/env python3
"""Prepare upstream snapshots explicitly; restore ordinary CI inputs from our release only."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
UPSTREAMS = ROOT / "ci/upstreams.json"
LOCK = ROOT / "ci/dependencies-lock.json"


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def verify(path, expected):
    if not path.is_file() or digest(path) != expected:
        raise ValueError("Dependency hash mismatch or missing file: {}".format(path))


def selected(platform):
    return [item for item in json.loads(UPSTREAMS.read_text()).values()
            if item.get("platform", platform) == platform]


def audit():
    """Keep ordinary developer builds and prepared CI sources on the same versions."""
    native = (ROOT / "CMakeLists.txt").read_text() + (ROOT / "cmake/speech.cmake").read_text()
    for item in json.loads(UPSTREAMS.read_text()).values():
        if item.get("source") not in (None, "voice") and item["sha256"] not in native:
            raise ValueError("Native dependency changed; update ci/upstreams.json: " + item["file"])


def extract(archive, destination):
    destination.mkdir(parents=True, exist_ok=True)
    if zipfile.is_zipfile(archive):
        with zipfile.ZipFile(archive) as source:
            for member in source.infolist():
                if not (destination / member.filename).resolve().is_relative_to(destination.resolve()):
                    raise ValueError("Unsafe archive path: " + member.filename)
            source.extractall(destination)
    else:
        with tarfile.open(archive) as source:
            source.extractall(destination, filter="data")


def unpack_root(archive, destination):
    with tempfile.TemporaryDirectory() as temporary:
        directory = Path(temporary)
        extract(archive, directory)
        entries = list(directory.iterdir())
        if len(entries) != 1 or not entries[0].is_dir():
            raise ValueError("Expected one archive root: " + str(archive))
        shutil.copytree(entries[0], destination, dirs_exist_ok=True)


def upstream(output, repository):
    """This command is used only by the manual dependency preparation workflow."""
    audit()
    output.mkdir(parents=True, exist_ok=True)
    manifest = json.loads(UPSTREAMS.read_text())
    for item in manifest.values():
        path = output / item["file"]
        if path.is_file() and digest(path) == item["sha256"]:
            continue
        if "release" in item:
            subprocess.run(["gh", "release", "download", item["release"], "--repo", repository,
                            "--pattern", item["file"], "--dir", str(output), "--clobber"], check=True)
        else:
            subprocess.run(["curl", "--fail", "--location", "--retry", "4", "--retry-all-errors",
                            "--output", str(path), item["url"]], check=True)
        verify(path, item["sha256"])
    for platform in ("Linux", "macOS", "Windows"):
        with tarfile.open(output / ("inputs-" + platform + ".tar.gz"), "w:gz", compresslevel=1) as bundle:
            bundle.add(UPSTREAMS, arcname="upstreams.json")
            for item in selected(platform):
                bundle.add(output / item["file"], arcname=item["file"])


def make_lock(output, repository, release):
    names = ["inputs-Linux.tar.gz", "inputs-macOS.tar.gz", "inputs-Windows.tar.gz",
             "Linux-environment.tar.gz", "macOS-tools.tar.gz"]
    lock = {"repository": repository, "release": release, "upstreams_sha256": digest(UPSTREAMS),
            "assets": {name: digest(output / name) for name in names}}
    (output / "dependencies-lock.json").write_text(json.dumps(lock, indent=2) + "\n")


def fetch_snapshot(names, downloads):
    audit()
    lock = json.loads(LOCK.read_text())
    if lock["upstreams_sha256"] != digest(UPSTREAMS):
        raise ValueError("Upstreams changed. Prepare and pin a new dependency snapshot first.")
    downloads.mkdir(parents=True, exist_ok=True)
    for name in names:
        destination = downloads / name
        expected = lock["assets"][name]
        if not destination.is_file() or digest(destination) != expected:
            # Never fall back to an upstream URL, including after cache eviction.
            with tempfile.TemporaryDirectory() as temporary:
                subprocess.run(["gh", "release", "download", lock["release"], "--repo", lock["repository"],
                                "--pattern", name, "--dir", temporary], check=True)
                verify(Path(temporary) / name, expected)
                shutil.copyfile(Path(temporary) / name, destination)
        verify(destination, expected)
    return lock


def restore(platform, output, package=False):
    names = ["inputs-" + platform + ".tar.gz"]
    if platform == "Linux":
        names.append("Linux-environment.tar.gz")
    elif platform == "macOS":
        names.append("macOS-tools.tar.gz")
    downloads = output / "downloads"
    lock = fetch_snapshot(names, downloads)
    archives = output / "archives"
    extract(downloads / names[0], archives)
    verify(archives / "upstreams.json", lock["upstreams_sha256"])
    for item in selected(platform):
        path = archives / item["file"]
        verify(path, item["sha256"])
        component = item.get("source")
        if component == "voice":
            if package:
                extract(path, ROOT / "build/kokoro-pack")
        elif component == "ortho_audio":
            target = output / "sources" / component
            target.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, target / "miniaudio.h")
        elif component:
            unpack_root(path, output / "sources" / component)
    if platform == "Linux":
        target = output / "tools/appimage"
        target.mkdir(parents=True, exist_ok=True)
        for name in ("linuxdeploy-x86_64.AppImage", "linuxdeploy-plugin-gtk.sh"):
            shutil.copyfile(archives / name, target / name)
            (target / name).chmod(0o755)
    elif platform == "Windows":
        tools = output / "tools"
        tools.mkdir(parents=True, exist_ok=True)
        subprocess.run(["7z", "x", str(archives / "w64devkit.7z.exe"), "-o" + str(tools), "-y"], check=True)
        unpack_root(archives / "nsis.zip", tools / "nsis")
        unpack_root(archives / "ccache-windows.zip", tools / "ccache")
    else:
        extract(downloads / names[1], output / "tools")
    print("Restored verified {} inputs from {}/{}".format(platform, lock["repository"], lock["release"]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["upstream", "lock", "restore", "reuse", "audit"])
    parser.add_argument("--output", type=Path, default=ROOT / "build/ci")
    parser.add_argument("--platform", choices=["Linux", "macOS", "Windows"])
    parser.add_argument("--package", action="store_true")
    parser.add_argument("--repository", default="jsjoberg/perikop")
    parser.add_argument("--release")
    args = parser.parse_args()
    if args.command == "upstream":
        upstream(args.output, args.repository)
    elif args.command == "lock":
        if not args.release:
            parser.error("lock requires --release")
        make_lock(args.output, args.repository, args.release)
    elif args.command == "restore":
        if not args.platform:
            parser.error("restore requires --platform")
        restore(args.platform, args.output, args.package)
    elif args.command == "reuse":
        if args.platform == "Linux":
            names = ["Linux-environment.tar.gz"]
        elif args.platform is None:
            names = ["inputs-" + platform + ".tar.gz" for platform in ("Linux", "macOS", "Windows")]
        else:
            parser.error("reuse accepts --platform Linux or no platform")
        fetch_snapshot(names, args.output)
    else:
        audit()


if __name__ == "__main__":
    main()
