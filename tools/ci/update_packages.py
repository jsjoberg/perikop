#!/usr/bin/env python3
"""Restore verified delta bases, collect Velopack outputs, and preserve feeds during nightly cleanup."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
PLATFORMS = {"Linux": "linux-x64", "Linux-arm64": "linux-arm64", "Windows": "win-x64",
             "macOS-arm64": "osx-arm64"}
NIGHTLY = re.compile(r"nightly-\d{4}-\d{2}-\d{2}(?:-[0-9a-f]{7}(?:-\d+(?:-\d+)?)?)?")
SEMVER = re.compile(r"(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-([0-9A-Za-z.-]+))?")


def precedence(version):
    match = SEMVER.fullmatch(version)
    if not match:
        raise ValueError("Invalid package version: " + version)
    pre = match[4]
    identifiers = []
    if pre:
        for part in pre.split("."):
            if not part or (part.isdigit() and len(part) > 1 and part.startswith("0")):
                raise ValueError("Invalid package version: " + version)
            identifiers.append((0, int(part)) if part.isdigit() else (1, part))
    return tuple(int(match[index]) for index in (1, 2, 3)), pre is None, identifiers


def build_version():
    version = os.environ.get("PERIKOP_BUILD_VERSION")
    if not version:
        ref = os.environ.get("GITHUB_REF", "")
        if ref.startswith("refs/tags/v"):
            version = ref[len("refs/tags/v"):]
        else:
            base = re.search(r"project\(perikop VERSION ([0-9.]+)", (ROOT / "CMakeLists.txt").read_text())[1]
            version = base + "-" + os.environ["GITHUB_SHA"][:7]
    precedence(version)
    return version


def channel_for(platform):
    return PLATFORMS[platform] + ("-nightly" if os.environ.get("NIGHTLY") == "true" else "-releases")


def check_channel(channel):
    # Retained Intel feeds remain readable for cleanup; assemble rejects new Intel publications.
    if channel not in {platform + "-" + track for platform in (*PLATFORMS.values(), "osx-x64")
                       for track in ("nightly", "releases")}:
        raise ValueError("Unknown update channel: " + channel)


def feed_assets(feed, channel):
    check_channel(channel)
    assets = feed["Assets"]
    if not isinstance(assets, list):
        raise ValueError("Invalid feed assets")
    seen = set()
    for asset in assets:
        version = asset["Version"]
        precedence(version)
        kind = asset["Type"]
        if kind not in ("Full", "Delta") or asset["PackageId"] != "Perikop":
            raise ValueError("Unexpected update asset")
        name = f"Perikop-{version}-{channel}-{kind.lower()}.nupkg"
        if asset["FileName"] != name or name in seen:
            raise ValueError("Invalid or duplicate update filename: " + asset["FileName"])
        seen.add(name)
        if not re.fullmatch(r"[0-9A-Fa-f]{64}", asset["SHA256"]) or not isinstance(asset["Size"], int) \
                or isinstance(asset["Size"], bool) or asset["Size"] <= 0:
            raise ValueError("Invalid package checksum or size")
    return assets


def verify(path, asset):
    if not path.is_file() or path.stat().st_size != asset["Size"]:
        raise ValueError("Missing package or incorrect size: " + str(path))
    with path.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    if digest.lower() != asset["SHA256"].lower():
        raise ValueError("Package checksum mismatch: " + str(path))


def gh(*arguments):
    return subprocess.check_output(["gh", *map(str, arguments)], text=True)


def releases(repository):
    pages = json.loads(gh("api", "--paginate", "--slurp", f"repos/{repository}/releases?per_page=100"))
    return [release for page in pages for release in page if not release["draft"]]


def download(repository, tag, name, destination):
    # All downloads stay within the chosen repository; feed URLs are never executed or fetched.
    if Path(name).name != name or name.startswith("-"):
        raise ValueError("Unsafe release asset name")
    gh("release", "download", tag, "--repo", repository, "--pattern", name, "--dir", destination)
    return destination / name


def restore(repository, platform, output, version):
    channel = channel_for(platform)
    target = precedence(version)
    output = output / channel
    output.mkdir(parents=True, exist_ok=True)
    if list(output.iterdir()):
        raise ValueError("Delta base directory must be empty: " + str(output))
    name = f"releases.{channel}.json"
    best = None
    for release in releases(repository):
        if channel.endswith("-releases") and release["prerelease"]:
            continue
        if name not in {asset["name"] for asset in release["assets"]}:
            continue
        with tempfile.TemporaryDirectory() as temporary:
            feed = json.loads(download(repository, release["tag_name"], name, Path(temporary)).read_text())
        for asset in feed_assets(feed, channel):
            key = precedence(asset["Version"])
            if asset["Type"] == "Full" and key < target and (best is None or key > best[0]):
                best = key, asset
    if best is None:
        print(f"{channel}: no earlier published full package; the first package has no delta")
        return
    asset = best[1]
    # A feed may name a base hosted by another release, including a base rescued during cleanup.
    host = next((release for release in releases(repository)
                 if asset["FileName"] in {item["name"] for item in release["assets"]}), None)
    if host is None:
        raise ValueError("Published delta base is missing: " + asset["FileName"])
    path = download(repository, host["tag_name"], asset["FileName"], output)
    verify(path, asset)
    (output / name).write_text(json.dumps({"Assets": [asset]}) + "\n")
    print(f"{channel}: verified delta base {asset['Version']}")


def seed(base, output):
    output.mkdir(parents=True, exist_ok=True)
    if list(output.iterdir()):
        raise ValueError("Package output must be empty before seeding: " + str(output))
    if base.exists():
        for path in base.iterdir():
            if path.is_file() and (path.name.endswith("-full.nupkg") or path.name.startswith("releases.")):
                shutil.copy2(path, output / path.name)


def finish(directory, channel, version, destination):
    assets = feed_assets(json.loads((directory / f"releases.{channel}.json").read_text()), channel)
    current = [asset for asset in assets if asset["Version"] == version]
    bases = [asset for asset in assets if asset["Type"] == "Full" and precedence(asset["Version"]) < precedence(version)]
    if len([asset for asset in current if asset["Type"] == "Full"]) != 1:
        raise ValueError("The package feed must contain its current full package")
    if bases and not any(asset["Type"] == "Delta" for asset in current):
        raise ValueError("A previous full package exists, but no delta was produced")
    if any(asset["Type"] == "Delta" for asset in current) and not bases:
        raise ValueError("A delta needs its previous full package")
    publication = json.loads((directory / f"assets.{channel}.json").read_text())
    names = set()
    for item in publication:
        name, kind = item["RelativeFileName"], item["Type"]
        if Path(name).name != name or not name.startswith("Perikop-") or kind not in ("Full", "Delta", "Installer", "Portable"):
            raise ValueError("Unexpected packaging output: " + name)
        if name in names or not (directory / name).is_file():
            raise ValueError("Missing or duplicate packaging output: " + name)
        names.add(name)
    for asset in current:
        if asset["FileName"] not in names:
            raise ValueError("The feed and packaging outputs disagree")
        verify(directory / asset["FileName"], asset)
    for asset in bases:
        verify(directory / asset["FileName"], asset)
    # Each publication includes only its new delta. Repeating older deltas in GitHub feeds would
    # make the pinned SDK try to apply the same patch twice when it combines release feeds.
    feed = {"Assets": bases + current}
    destination.mkdir(parents=True, exist_ok=True)
    for name in names:
        if (destination / name).exists():
            raise ValueError("Duplicate publication filename: " + name)
        shutil.copy2(directory / name, destination / name)
    (destination / f"releases.{channel}.json").write_text(json.dumps(feed) + "\n")


def assemble(legacy, updates, output, version):
    files = list(legacy.glob("Perikop-*"))
    # The AppImage runtime statically links libfuse (LGPL-2.1); its corresponding source ships beside the AppImages.
    runtime_sources = list(legacy.glob("AppImage-runtime-source-*.tar.gz"))
    if any(file.name.endswith(".AppImage") for file in files) and len(runtime_sources) != 1:
        raise ValueError("AppImages need the corresponding source of their runtime; see tools/appimage_runtime.py")
    feeds = sorted(updates.glob("releases.*.json"))
    if not files or not feeds:
        raise ValueError("Publication needs native packages and update feeds")
    expected = {"linux-x64": "x86_64.AppImage", "linux-arm64": "aarch64.AppImage", "win-x64": "windows-x64.exe",
                "osx-arm64": "macOS-arm64.dmg"}
    present = set()
    for feed in feeds:
        channel = feed.name[len("releases."):-len(".json")]
        platform = channel.rsplit("-", 1)[0]
        if platform not in expected:
            raise ValueError("Publication contains a retired platform: " + platform)
        assets = feed_assets(json.loads(feed.read_text()), channel)
        if not any(asset["Type"] == "Full" and asset["Version"] == version for asset in assets):
            raise ValueError("Feed version does not match the build: " + feed.name)
        if platform in present:
            raise ValueError("Duplicate platform feeds")
        present.add(platform)
        if not any(file.name.endswith("-" + expected[platform]) for file in files):
            raise ValueError("Update feed lacks its native platform package")
        for asset in assets:
            if asset["Version"] == version:
                verify(updates / asset["FileName"], asset)
    if len(files) != len(present):
        raise ValueError("Native packages and update feeds cover different platforms")
    output.mkdir(parents=True, exist_ok=True)
    if list(output.iterdir()):
        raise ValueError("Release staging directory must be empty")
    for file in files + runtime_sources + list(updates.iterdir()):
        if not file.is_file() or (output / file.name).exists():
            raise ValueError("Invalid or duplicate release file: " + file.name)
        shutil.copy2(file, output / file.name)
    checksums = []
    for file in sorted(output.iterdir()):
        with file.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        checksums.append(digest + "  " + file.name + "\n")
    (output / "SHA256SUMS.txt").write_text("".join(checksums))


def retention_plan(all_releases, feeds, current, keep=7):
    if keep < 1 or keep > 9:
        raise ValueError("Keep between one and nine nightlies so the SDK can discover releases")
    nightlies = sorted((release for release in all_releases if not release["draft"] and release["prerelease"]
                        and NIGHTLY.fullmatch(release["tag_name"])),
                       key=lambda release: (release["published_at"], release["tag_name"]), reverse=True)
    retained = nightlies[:keep]
    if current not in {release["tag_name"] for release in retained}:
        raise ValueError("Published nightly is missing from the retained releases; cleanup stopped")
    available = {asset["name"] for release in retained for asset in release["assets"]}
    references = {}
    for release in retained:
        for item in release["assets"]:
            name = item["name"]
            if not name.startswith("releases.") or not name.endswith("-nightly.json"):
                continue
            channel = name[len("releases."):-len(".json")]
            for asset in feed_assets(feeds[(release["tag_name"], name)], channel):
                previous = references.setdefault(asset["FileName"], asset)
                if (previous["SHA256"].lower(), previous["Size"]) != (asset["SHA256"].lower(), asset["Size"]):
                    raise ValueError("Retained feeds disagree about an update package")
    rescue = []
    for name, asset in sorted(references.items()):
        if name in available:
            continue
        host = next((release for release in all_releases if not release["draft"]
                     and name in {item["name"] for item in release["assets"]}), None)
        if host is None:
            raise ValueError("A retained feed references a missing package: " + name)
        rescue.append((host["tag_name"], asset))
    return rescue, [release["tag_name"] for release in nightlies[keep:]]


def read_checksums(text):
    result = {}
    for line in text.splitlines():
        match = re.fullmatch(r"([0-9a-fA-F]{64})  (.+)", line)
        if not match or Path(match[2]).name != match[2] or match[2] in result or match[2] == "SHA256SUMS.txt":
            raise ValueError("Invalid release checksums")
        result[match[2]] = match[1].lower()
    if not result:
        raise ValueError("Release checksums are empty")
    return result


def checksum_text(checksums):
    return "".join(checksums[name] + "  " + name + "\n" for name in sorted(checksums))


def cleanup(repository, current, keep):
    all_releases = releases(repository)
    nightlies = sorted((release for release in all_releases if release["prerelease"]
                        and NIGHTLY.fullmatch(release["tag_name"])),
                       key=lambda release: (release["published_at"], release["tag_name"]), reverse=True)[:keep]
    feeds = {}
    with tempfile.TemporaryDirectory(prefix="perikop-nightly-cleanup-") as temporary:
        work = Path(temporary)
        for release in nightlies:
            directory = work / release["tag_name"]
            directory.mkdir()
            for asset in release["assets"]:
                name = asset["name"]
                if name.startswith("releases.") and name.endswith("-nightly.json"):
                    feeds[(release["tag_name"], name)] = json.loads(
                        download(repository, release["tag_name"], name, directory).read_text())
        rescue, obsolete = retention_plan(all_releases, feeds, current, keep)
        current_names = {item["name"] for release in all_releases if release["tag_name"] == current
                         for item in release["assets"]}
        checksum_dir = work / "checksums"
        checksum_dir.mkdir()
        checksum_file = download(repository, current, "SHA256SUMS.txt", checksum_dir)
        original_checksums = checksum_file.read_text()
        checksums = read_checksums(original_checksums)
        references = {asset["FileName"]: asset for feed in feeds.values() for asset in feed["Assets"]}
        for name in current_names - {"SHA256SUMS.txt"}:
            if name in checksums:
                if name in references and checksums[name] != references[name]["SHA256"].lower():
                    raise ValueError("Release checksums disagree with its feeds")
                continue
            # Repair an attempt that uploaded a rescued base but failed before replacing the
            # checksum list. Verify its bytes before adding its checksum.
            if name not in references:
                raise ValueError("An uploaded asset has no checksum or feed metadata: " + name)
            path = download(repository, current, name, checksum_dir)
            verify(path, references[name])
            checksums[name] = references[name]["SHA256"].lower()
        rescue_dir = work / "rescue"
        rescue_dir.mkdir()
        for tag, asset in rescue:
            path = download(repository, tag, asset["FileName"], rescue_dir)
            verify(path, asset)
            gh("release", "upload", current, path, "--repo", repository)
            checksums[asset["FileName"]] = asset["SHA256"].lower()
        updated_checksums = checksum_text(checksums)
        if updated_checksums != original_checksums:
            checksum_file.write_text(updated_checksums)
            gh("release", "upload", current, checksum_file, "--repo", repository, "--clobber")
        # Confirm every referenced rescue asset is present before deleting any old release/tag.
        names = {item["name"] for item in json.loads(gh("release", "view", current, "--repo", repository,
                                                       "--json", "assets"))["assets"]}
        if "SHA256SUMS.txt" not in names or set(checksums) - names:
            raise ValueError("Checksummed packages are missing; no nightly releases were deleted")
        for tag in obsolete:
            gh("release", "delete", tag, "--repo", repository, "--cleanup-tag", "--yes")
        print(f"Retained {min(keep, len(nightlies))} nightlies; rescued {len(rescue)} referenced packages")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("version")
    restoring = commands.add_parser("restore")
    restoring.add_argument("--repository", default=os.environ.get("GITHUB_REPOSITORY"), required=False)
    restoring.add_argument("--platform", choices=PLATFORMS, required=True)
    restoring.add_argument("--output", type=Path, default=ROOT / "build/update-base")
    restoring.add_argument("--version")
    seeding = commands.add_parser("seed")
    seeding.add_argument("base", type=Path)
    seeding.add_argument("output", type=Path)
    finishing = commands.add_parser("finish")
    finishing.add_argument("directory", type=Path)
    finishing.add_argument("channel")
    finishing.add_argument("version")
    finishing.add_argument("destination", type=Path)
    staging = commands.add_parser("assemble")
    for argument in ("legacy", "updates", "output"):
        staging.add_argument(argument, type=Path)
    staging.add_argument("version")
    cleaning = commands.add_parser("cleanup")
    cleaning.add_argument("--repository", required=True)
    cleaning.add_argument("--current", required=True)
    cleaning.add_argument("--keep", type=int, default=7)
    args = parser.parse_args()
    if args.command == "version":
        print(build_version())
    elif args.command == "restore":
        if not args.repository:
            parser.error("restore needs --repository or GITHUB_REPOSITORY")
        restore(args.repository, args.platform, args.output, args.version or build_version())
    elif args.command == "seed":
        seed(args.base, args.output)
    elif args.command == "finish":
        finish(args.directory, args.channel, args.version, args.destination)
    elif args.command == "assemble":
        assemble(args.legacy, args.updates, args.output, args.version)
    else:
        cleanup(args.repository, args.current, args.keep)


if __name__ == "__main__":
    main()
