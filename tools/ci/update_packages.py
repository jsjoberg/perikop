#!/usr/bin/env python3
"""Restore verified delta bases, collect Velopack outputs, publish them to the updates release, and trim nightlies."""

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
# Every channel's feed and packages live on one fixed release; the app reads it through Velopack's HTTP source.
UPDATES = "jsjoberg/perikop-assets"
UPDATES_TAG = "updates"
# Feeds keep the newest full package and the deltas of this many newest versions.
KEEP = 7
PLATFORMS = {"Linux": "linux-x64-glibc239", "Linux-arm64": "linux-arm64-glibc239", "Windows": "win-x64",
             "macOS-arm64": "osx-arm64"}
# What Velopack makes for people to install from, as Perikop-<channel><suffix>, beside each feed.
INSTALLERS = {"linux-x64-glibc239": ".AppImage", "linux-arm64-glibc239": ".AppImage", "win-x64": "-Setup.exe",
              "osx-arm64": "-Setup.pkg"}
# The plain names people download, by Velopack suffix.
DOWNLOADS = {"linux-x64-glibc239": {".AppImage": "Perikop-x86_64.AppImage"},
             "linux-arm64-glibc239": {".AppImage": "Perikop-aarch64.AppImage"},
             "win-x64": {"-Setup.exe": "Perikop-Setup.exe", "-Portable.zip": "Perikop-Windows-Portable.zip"},
             "osx-arm64": {"-Setup.pkg": "Perikop.pkg", "-Portable.zip": "Perikop-macOS-Portable.zip"}}
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
    # Retired feeds remain readable for cleanup; assemble rejects publishing to them.
    # nightly-2026-10-25-cab925d published the short-lived glibc241 Linux feeds.
    retired = ("osx-x64", "linux-x64", "linux-arm64", "linux-x64-glibc241", "linux-arm64-glibc241")
    if channel not in {platform + "-" + track for platform in (*PLATFORMS.values(), *retired)
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
    # shortcut: the main repository's releases hold the bases from before the updates release; drop them after 0.2.1.
    published = [(UPDATES, release) for release in releases(UPDATES) if release["tag_name"] == UPDATES_TAG]
    published += [(repository, release) for release in releases(repository)]
    for source, release in published:
        if channel.endswith("-releases") and release["prerelease"]:
            continue
        if name not in {asset["name"] for asset in release["assets"]}:
            continue
        with tempfile.TemporaryDirectory() as temporary:
            feed = json.loads(download(source, release["tag_name"], name, Path(temporary)).read_text())
        for asset in feed_assets(feed, channel):
            key = precedence(asset["Version"])
            if asset["Type"] == "Full" and key < target and (best is None or key > best[0]):
                best = key, asset, source
    if best is None:
        print(f"{channel}: no earlier published full package; the first package has no delta")
        return
    _, asset, source = best
    # A feed may name a base hosted by another release of its repository.
    host = next((release for repo, release in published if repo == source
                 and asset["FileName"] in {item["name"] for item in release["assets"]}), None)
    if host is None:
        raise ValueError("Published delta base is missing: " + asset["FileName"])
    path = download(source, host["tag_name"], asset["FileName"], output)
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


def assemble(updates, output, version, complete=False):
    feeds = sorted(updates.glob("releases.*.json"))
    if not feeds:
        raise ValueError("Publication needs update feeds")
    present = set()
    if len({feed.name.rsplit("-", 1)[1] for feed in feeds}) != 1:
        raise ValueError("Publication mixes release and nightly channels")
    for feed in feeds:
        channel = feed.name[len("releases."):-len(".json")]
        platform = channel.rsplit("-", 1)[0]
        if platform not in INSTALLERS:
            raise ValueError("Publication contains a retired platform: " + platform)
        assets = feed_assets(json.loads(feed.read_text()), channel)
        if not any(asset["Type"] == "Full" and asset["Version"] == version for asset in assets):
            raise ValueError("Feed version does not match the build: " + feed.name)
        if platform in present:
            raise ValueError("Duplicate platform feeds")
        present.add(platform)
        if not (updates / f"Perikop-{channel}{INSTALLERS[platform]}").is_file():
            raise ValueError("Update feed lacks its installer: " + channel)
        for asset in assets:
            if asset["Version"] == version:
                verify(updates / asset["FileName"], asset)
    if complete and present != set(INSTALLERS):
        raise ValueError("A release needs every platform; missing " + ", ".join(sorted(set(INSTALLERS) - present)))
    files = sorted(updates.iterdir())
    # The AppImage runtime statically links libfuse (LGPL-2.1); its corresponding source ships beside the AppImages.
    runtime_sources = [file for file in files if file.name.startswith("AppImage-runtime-source-")]
    if any(file.name.endswith(".AppImage") for file in files) and len(runtime_sources) != 1:
        raise ValueError("AppImages need the corresponding source of their runtime; see tools/appimage_runtime.py")
    output.mkdir(parents=True, exist_ok=True)
    if list(output.iterdir()):
        raise ValueError("Release staging directory must be empty")
    downloads, packages = output / "downloads", output / "packages"
    downloads.mkdir()
    packages.mkdir()
    renames = {f"Perikop-{platform}-{track}{suffix}": name for platform, names in DOWNLOADS.items()
               for suffix, name in names.items() for track in ("releases", "nightly")}
    for file in files:
        if not file.is_file():
            raise ValueError("Invalid release file: " + file.name)
        if file.name in renames:
            shutil.copy2(file, downloads / renames[file.name])
        elif file in runtime_sources:
            shutil.copy2(file, downloads / file.name)
        elif file.name.endswith(".nupkg") or file.name.startswith("releases."):
            shutil.copy2(file, packages / file.name)
            # shortcut: 0.2.1 also carries its feeds and packages so 0.2.0's GitHub source finds it; remove after 0.2.1.
            if version == "0.2.1":
                shutil.copy2(file, downloads / file.name)
        else:
            raise ValueError("Unexpected release file: " + file.name)
    checksums = []
    for file in sorted(downloads.iterdir()):
        with file.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        checksums.append(digest + "  " + file.name + "\n")
    (downloads / "SHA256SUMS.txt").write_text("".join(checksums))


def merge(assets, new):
    """The newest full package and the deltas of the KEEP newest versions.

    shortcut: the SDK applies every delta above the installed version, so a copy older than the oldest
    delta's base gets an incomplete chain and falls back to the full package only if patching fails, as
    with the GitHub source's ten-release window before; keep every delta if that becomes a problem.
    """
    versions = {asset["Version"] for asset in new}
    newest = sorted([asset for asset in assets if asset["Version"] not in versions] + new,
                    key=lambda asset: precedence(asset["Version"]), reverse=True)
    full = next(asset for asset in newest if asset["Type"] == "Full")
    return [full] + [asset for asset in newest if asset["Type"] == "Delta"][:KEEP]


def updates_assets():
    """The asset names on the updates release, which is created on first use."""
    found = [release for release in releases(UPDATES) if release["tag_name"] == UPDATES_TAG]
    if not found:
        gh("release", "create", UPDATES_TAG, "--repo", UPDATES, "--latest=false", "--title", "Perikop updates",
           "--notes", "Feeds and packages that installed copies of Perikop update from. "
           "Downloads are at https://github.com/jsjoberg/perikop/releases.")
        return set()
    return {item["name"] for item in found[0]["assets"]}


def feed_channel(name):
    return name[name.index("releases.") + len("releases."):-len(".json")]


def stage(packages, tag, version):
    """Upload the new packages, and the feed entries that publish adds as <tag>.releases.<channel>.json."""
    names = updates_assets()
    for feed in sorted(packages.glob("releases.*.json")):
        channel = feed_channel(feed.name)
        current = [asset for asset in feed_assets(json.loads(feed.read_text()), channel) if asset["Version"] == version]
        for asset in current:
            path = packages / asset["FileName"]
            verify(path, asset)
            if asset["FileName"] not in names:
                gh("release", "upload", UPDATES_TAG, path, "--repo", UPDATES)
                continue
            # A retry: a live feed may already name these bytes, so they are never replaced.
            with tempfile.TemporaryDirectory() as temporary:
                verify(download(UPDATES, UPDATES_TAG, asset["FileName"], Path(temporary)), asset)
        staged = packages / f"{tag}.{feed.name}"
        staged.write_text(json.dumps({"Assets": current}) + "\n")
        gh("release", "upload", UPDATES_TAG, staged, "--repo", UPDATES, "--clobber")


def publish(tag):
    """Merge the entries staged for tag into the live feeds, then delete the packages no feed names."""
    names = updates_assets()
    staged = sorted(name for name in names if name.startswith(tag + ".releases."))
    if not staged:
        print(f"Nothing is staged for {tag}")
    with tempfile.TemporaryDirectory(prefix="perikop-updates-") as temporary:
        work = Path(temporary)
        for name in staged:
            live, channel = name[len(tag) + 1:], feed_channel(name)
            directory = work / name
            directory.mkdir()
            new = feed_assets(json.loads(download(UPDATES, UPDATES_TAG, name, directory).read_text()), channel)
            for asset in new:
                verify(download(UPDATES, UPDATES_TAG, asset["FileName"], directory), asset)
            assets = []
            if live in names:
                assets = feed_assets(json.loads(download(UPDATES, UPDATES_TAG, live, directory).read_text()), channel)
            feed = work / live
            feed.write_text(json.dumps({"Assets": merge(assets, new)}) + "\n")
            gh("release", "upload", UPDATES_TAG, feed, "--repo", UPDATES, "--clobber")
            gh("release", "delete-asset", UPDATES_TAG, name, "--repo", UPDATES, "--yes")
            print(f"{channel}: published {new[0]['Version']}")
        # Packages named by live or still-staged feeds stay.
        names = updates_assets()
        referenced = set()
        for name in names:
            if name.endswith(".json") and "releases." in name:
                directory = work / "feeds" / name
                directory.mkdir(parents=True)
                feed = json.loads(download(UPDATES, UPDATES_TAG, name, directory).read_text())
                referenced |= {asset["FileName"] for asset in feed_assets(feed, feed_channel(name))}
        for name in sorted(names):
            if name.endswith(".nupkg") and name not in referenced:
                gh("release", "delete-asset", UPDATES_TAG, name, "--repo", UPDATES, "--yes")


def cleanup(repository, keep):
    """Delete all but the newest nightlies; installed copies update from the updates release."""
    nightlies = sorted((release for release in releases(repository) if release["prerelease"]
                        and NIGHTLY.fullmatch(release["tag_name"])),
                       key=lambda release: (release["published_at"], release["tag_name"]), reverse=True)
    for release in nightlies[keep:]:
        gh("release", "delete", release["tag_name"], "--repo", repository, "--cleanup-tag", "--yes")
    print(f"Retained {min(keep, len(nightlies))} nightlies")


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
    for argument in ("updates", "output"):
        staging.add_argument(argument, type=Path)
    staging.add_argument("version")
    staging.add_argument("--complete", action="store_true", help="Require every platform, as a release does")
    uploading = commands.add_parser("stage")
    uploading.add_argument("packages", type=Path)
    uploading.add_argument("--tag", required=True)
    uploading.add_argument("--version", required=True)
    publishing = commands.add_parser("publish")
    publishing.add_argument("--tag", required=True)
    cleaning = commands.add_parser("cleanup")
    cleaning.add_argument("--repository", required=True)
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
        assemble(args.updates, args.output, args.version, args.complete)
    elif args.command == "stage":
        stage(args.packages, args.tag, args.version)
    elif args.command == "publish":
        publish(args.tag)
    else:
        cleanup(args.repository, args.keep)


if __name__ == "__main__":
    main()
