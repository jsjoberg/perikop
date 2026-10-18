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
PLATFORMS = ("Linux", "Linux-arm64", "macOS", "Windows")
CROSS_ENVIRONMENT = "Windows-cross-environment.tar.gz"
# The AppImage tools for each Linux platform, named for the processor they run on.
APPIMAGE_TOOLS = {
    "Linux": ("linuxdeploy-x86_64.AppImage", "linuxdeploy-plugin-gtk.sh", "runtime-x86_64"),
    "Linux-arm64": ("linuxdeploy-aarch64.AppImage", "linuxdeploy-plugin-gtk.sh", "runtime-aarch64"),
}


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
    """Items for one platform. An item without a platform serves all; a list serves several."""
    def serves(item):
        wanted = item.get("platform", platform)
        return platform in wanted if isinstance(wanted, list) else wanted == platform
    return [item for item in json.loads(UPSTREAMS.read_text()).values() if serves(item)]


def audit():
    """Keep ordinary developer builds and prepared CI sources on the same versions."""
    native = "".join((ROOT / name).read_text() for name in ("CMakeLists.txt", "cmake/speech.cmake", "cmake/updates.cmake"))
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
    for platform in PLATFORMS:
        with tarfile.open(output / ("inputs-" + platform + ".tar.gz"), "w:gz", compresslevel=1) as bundle:
            bundle.add(UPSTREAMS, arcname="upstreams.json")
            for item in selected(platform):
                bundle.add(output / item["file"], arcname=item["file"])


def make_lock(output, repository, release):
    names = ["inputs-" + platform + ".tar.gz" for platform in PLATFORMS]
    names += ["Linux-environment.tar.gz", "Linux-arm64-environment.tar.gz", "macOS-tools.tar.gz"]
    if (output / CROSS_ENVIRONMENT).is_file():
        names.append(CROSS_ENVIRONMENT)
    lock = {"repository": repository, "release": release, "upstreams_sha256": digest(UPSTREAMS),
            "assets": {name: digest(output / name) for name in names}}
    (output / "dependencies-lock.json").write_text(json.dumps(lock, indent=2) + "\n")


def fetch_snapshot(names, downloads, validate_upstreams=True):
    audit()
    lock = json.loads(LOCK.read_text())
    if validate_upstreams and lock["upstreams_sha256"] != digest(UPSTREAMS):
        raise ValueError("Upstreams changed. Prepare and pin a new dependency snapshot first.")
    for name in names:
        if name not in lock["assets"]:
            raise ValueError("Dependency snapshot lacks {}. Prepare and pin a new dependency snapshot first.".format(name))
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


def restore(platform, output, package=False, cross=False, tools_only=False):
    if cross and platform != "Windows":
        raise ValueError("Cross-build inputs require --platform Windows")
    if tools_only and (platform != "Windows" or cross or package):
        raise ValueError("Tools-only restore requires Windows without --cross or --package")
    names = ["inputs-" + platform + ".tar.gz"]
    if cross:
        names.append(CROSS_ENVIRONMENT)
    elif platform in APPIMAGE_TOOLS:
        names.append(platform + "-environment.tar.gz")
    elif platform == "macOS":
        names.append("macOS-tools.tar.gz")
    downloads = output / "downloads"
    lock = fetch_snapshot(names, downloads)
    archives = output / "archives"
    extract(downloads / names[0], archives)
    verify(archives / "upstreams.json", lock["upstreams_sha256"])
    for item in selected(platform):
        if tools_only and item["file"] not in ("nsis.zip", "vpk.nupkg"):
            continue
        path = archives / item["file"]
        verify(path, item["sha256"])
        component = item.get("source")
        if component == "voice":
            if package:
                extract(path, ROOT / "build/kokoro-pack")
        elif component == "ortho_velopack":
            # The SDK archive holds include/, lib/, and lib-static/ side by side.
            extract(path, output / "sources" / component)
        elif component == "ortho_audio":
            target = output / "sources" / component
            target.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, target / "miniaudio.h")
        elif component:
            unpack_root(path, output / "sources" / component)
    if cross:
        # Windows sources are shared with native builds. Its executable tools are not.
        pass
    elif platform in APPIMAGE_TOOLS:
        target = output / "tools/appimage"
        target.mkdir(parents=True, exist_ok=True)
        for name in APPIMAGE_TOOLS[platform]:
            shutil.copyfile(archives / name, target / name)
            (target / name).chmod(0o755)
    elif platform == "Windows":
        tools = output / "tools"
        tools.mkdir(parents=True, exist_ok=True)
        if not tools_only:
            subprocess.run(["7z", "x", str(archives / "w64devkit.7z.exe"), "-o" + str(tools), "-y"], check=True)
        unpack_root(archives / "nsis.zip", tools / "nsis")
        if not tools_only:
            unpack_root(archives / "ccache-windows.zip", tools / "ccache")
    else:
        extract(downloads / names[1], output / "tools")
    # Zip extraction drops executable bits on the native Linux and macOS helpers.
    if not cross and any(item["file"] == "vpk.nupkg" for item in selected(platform)):
        vpk = output / "tools/vpk"
        extract(archives / "vpk.nupkg", vpk)
        for helper in (vpk / "vendor").iterdir():
            if helper.is_file():
                with helper.open("rb") as stream:
                    magic = stream.read(4)
                if magic in (b"\x7fELF", b"\xca\xfe\xba\xbe", b"\xcf\xfa\xed\xfe"):
                    helper.chmod(0o755)
    print("Restored verified {} inputs from {}/{}".format(platform, lock["repository"], lock["release"]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["upstream", "lock", "restore", "reuse", "audit"])
    parser.add_argument("--output", type=Path, default=ROOT / "build/ci")
    parser.add_argument("--platform", choices=PLATFORMS + ("Windows-cross",))
    parser.add_argument("--package", action="store_true")
    parser.add_argument("--cross", action="store_true", help="Restore Windows sources and the Linux cross-build environment")
    parser.add_argument("--tools-only", action="store_true", help="Restore only NSIS and vpk for prebuilt Windows packaging")
    parser.add_argument("--repository", default="jsjoberg/perikop-assets")
    parser.add_argument("--release")
    args = parser.parse_args()
    if args.command == "upstream":
        upstream(args.output, args.repository)
    elif args.command == "lock":
        if not args.release:
            parser.error("lock requires --release")
        make_lock(args.output, args.repository, args.release)
    elif args.command == "restore":
        if args.platform not in PLATFORMS:
            parser.error("restore requires --platform Linux, Linux-arm64, macOS, or Windows")
        restore(args.platform, args.output, args.package, args.cross, args.tools_only)
    elif args.command == "reuse":
        if args.platform in APPIMAGE_TOOLS:
            names = [args.platform + "-environment.tar.gz"]
        elif args.platform == "macOS":
            names = ["macOS-tools.tar.gz"]
        elif args.platform == "Windows-cross":
            names = [CROSS_ENVIRONMENT]
        elif args.platform is None:
            names = ["inputs-" + platform + ".tar.gz" for platform in PLATFORMS]
        else:
            parser.error("reuse accepts --platform Linux, Linux-arm64, macOS, Windows-cross, or no platform")
        # Environment snapshots are independent of the native source manifest.
        fetch_snapshot(names, args.output, validate_upstreams=args.platform is None)
    else:
        audit()


if __name__ == "__main__":
    main()
