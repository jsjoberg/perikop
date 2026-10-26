#!/usr/bin/env python3
"""Stage, verify and promote Perikop's paired download/update releases."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile

import update_packages as packages

MARKER = "<!-- perikop-update-source: jsjoberg/perikop-updates -->"
ARM64_REQUIREMENTS = (
    "Raspberry Pi 3, 4 and 5 use the same ARM64 download with 64-bit Raspberry Pi OS "
    "Trixie (Debian 13) and a desktop. It requires glibc 2.39 or later; 32-bit Raspberry Pi OS "
    "and Bookworm are not compatible. For read-aloud, install eSpeak NG separately "
    "(`sudo apt install libespeak-ng1`). Pi 3/4 compatibility is expected; physical-device "
    "performance and audio have not yet been verified."
)


def installer_name(platform, version):
    packages.precedence(version)
    return f"Perikop-{platform}-{version}{packages.INSTALLERS[platform]}"


def checksums(directory):
    values = {}
    for file in sorted(directory.iterdir()):
        if file.name != "SHA256SUMS.txt":
            with file.open("rb") as stream:
                values[file.name] = hashlib.file_digest(stream, "sha256").hexdigest()
    (directory / "SHA256SUMS.txt").write_text(packages.checksum_text(values))


class Repository:
    def __init__(self, name, token):
        if not token:
            raise ValueError("Missing release credential for " + name)
        self.name, self.token = name, token

    def gh(self, *arguments):
        return packages.gh(*arguments, token=self.token)

    def releases(self):
        pages = json.loads(self.gh("api", "--paginate", "--slurp", f"repos/{self.name}/releases?per_page=100"))
        return [release for page in pages for release in page]

    def download(self, tag, name, destination):
        return packages.download(self.name, tag, name, destination, token=self.token)

    def publication_target(self, tag, source):
        # Release dates can follow the tagged commit; avoid dating every storage tag to the initial README.
        # This commit is deliberately held by its release tag, without advancing main.
        parent = json.loads(self.gh("api", f"repos/{self.name}/commits/main"))
        commit = json.loads(self.gh("api", "--method", "POST", f"repos/{self.name}/git/commits",
                                   "-f", f"message=Perikop {tag} from {source}",
                                   "-f", "tree=" + parent["commit"]["tree"]["sha"],
                                   "-f", "parents[]=" + parent["sha"]))
        return commit["sha"]

    def draft(self, tag, directory, notes, nightly, target):
        existing = next((item for item in self.releases() if item["tag_name"] == tag), None)
        if existing and not existing["draft"]:
            # A retry may resume after the update release was promoted. Never replace public bytes.
            self.verify(tag, directory)
            return
        if not existing:
            if self.name == packages.UPDATE_REPOSITORY:
                target = self.publication_target(tag, target)
            options = ["--prerelease"] if nightly else []
            self.gh("release", "create", tag, "--repo", self.name, "--draft", "--latest=false",
                    "--target", target, "--title", "Perikop " + tag, "--notes-file", notes, *options)
        else:
            expected = {file.name for file in directory.iterdir()}
            if {item["name"] for item in existing["assets"]} - expected:
                raise ValueError("Draft has unexpected assets; inspect it before retrying: " + self.name)
            self.gh("release", "edit", tag, "--repo", self.name, "--notes-file", notes)
        self.gh("release", "upload", tag, "--repo", self.name, "--clobber", *sorted(directory.iterdir()))
        self.verify(tag, directory)

    def verify(self, tag, expected=None):
        with tempfile.TemporaryDirectory(prefix="perikop-release-verify-") as temporary:
            output = Path(temporary)
            self.gh("release", "download", tag, "--repo", self.name, "--dir", output)
            manifest = packages.read_checksums((output / "SHA256SUMS.txt").read_text())
            if set(manifest) != {path.name for path in output.iterdir()} - {"SHA256SUMS.txt"}:
                raise ValueError("Remote release and checksum list disagree: " + self.name)
            for name, digest in manifest.items():
                with (output / name).open("rb") as stream:
                    if hashlib.file_digest(stream, "sha256").hexdigest() != digest:
                        raise ValueError("Remote checksum mismatch: " + name)
            if expected and (output / "SHA256SUMS.txt").read_bytes() != (expected / "SHA256SUMS.txt").read_bytes():
                raise ValueError("Remote release differs from staging: " + self.name)
            referenced = set()
            for feed in output.glob("releases.*.json"):
                channel = feed.name[len("releases."):-len(".json")]
                for asset in packages.feed_assets(json.loads(feed.read_text()), channel):
                    packages.verify(output / asset["FileName"], asset)
                    referenced.add(asset["FileName"])
            if self.name == packages.UPDATE_REPOSITORY and any(
                    name.endswith(".nupkg") and name not in referenced for name in manifest):
                raise ValueError("Update package is not referenced by its feed")
            return manifest

    def promote(self, tag, nightly):
        self.gh("release", "edit", tag, "--repo", self.name, "--draft=false",
                "--latest=false" if nightly or self.name == packages.UPDATE_REPOSITORY else "--latest=true")


def published(repository):
    return [item for item in repository.releases() if not item["draft"]]


def full_snapshot(repository, releases, channel, destination):
    """Carry only the newest full package; repeating historical deltas breaks SDK merging."""
    name = f"releases.{channel}.json"
    best = None
    for release in releases:
        if channel.endswith("-releases") and release["prerelease"]:
            continue
        if name not in {item["name"] for item in release["assets"]}:
            continue
        with tempfile.TemporaryDirectory() as temporary:
            feed = repository.download(release["tag_name"], name, Path(temporary))
            assets = packages.feed_assets(json.loads(feed.read_text()), channel)
        for asset in assets:
            if asset["Type"] == "Full" and (best is None or packages.precedence(asset["Version"]) > best[0]):
                best = packages.precedence(asset["Version"]), asset, release
    if best is None:
        return False
    _, asset, _ = best
    host = next((release for release in releases
                 if asset["FileName"] in {item["name"] for item in release["assets"]}), None)
    if host is None:
        raise ValueError("Missing snapshot package: " + asset["FileName"])
    path = destination / asset["FileName"]
    if not path.exists():
        repository.download(host["tag_name"], asset["FileName"], destination)
    packages.verify(path, asset)
    if channel.startswith("linux-"):
        copy_sources(repository, host, destination)
    (destination / name).write_text(json.dumps({"Assets": [asset]}) + "\n")
    return True


def copy_sources(repository, release, destination):
    names = [item["name"] for item in release["assets"] if item["name"].startswith("AppImage-runtime-source-")]
    if not names:
        return
    with tempfile.TemporaryDirectory() as temporary:
        work = Path(temporary)
        manifest = packages.read_checksums(repository.download(release["tag_name"], "SHA256SUMS.txt", work).read_text())
        for name in names:
            if name not in manifest:
                raise ValueError("Runtime source missing from checksum manifest: " + name)
            path = destination / name
            if not path.exists():
                repository.download(release["tag_name"], name, destination)
            with path.open("rb") as stream:
                if hashlib.file_digest(stream, "sha256").hexdigest() != manifest[name]:
                    raise ValueError("Runtime source checksum mismatch: " + name)


def split(updates, output, version, complete):
    """Validate the build before moving the primary downloads out of internal storage."""
    with tempfile.TemporaryDirectory() as temporary:
        assembled = Path(temporary) / "assembled"
        packages.assemble(updates, assembled, version, complete)
        main, internal = output / "downloads", output / "updates"
        main.mkdir(parents=True)
        internal.mkdir()
        for path in assembled.iterdir():
            if path.name == "SHA256SUMS.txt":
                continue
            platform = next((platform for platform, suffix in packages.INSTALLERS.items()
                             if path.name in (f"Perikop-{platform}-releases{suffix}",
                                              f"Perikop-{platform}-nightly{suffix}")), None)
            destination = main / installer_name(platform, version) if platform else internal / path.name
            shutil.copy2(path, destination)
        checksums(main)
        checksums(internal)
    return main, internal


def carry_feeds(repository, releases, internal):
    # Carry source archives forward even after the nightly that first hosted them is removed.
    previous = next((release for release in releases if any(
        item["name"].startswith("AppImage-runtime-source-") for item in release["assets"])), None)
    if previous:
        copy_sources(repository, previous, internal)
    # A partial nightly must not age another platform's last working feed out of the SDK window.
    for platform in packages.INSTALLERS:
        for track in ("releases", "nightly"):
            channel = platform + "-" + track
            if not (internal / f"releases.{channel}.json").exists():
                full_snapshot(repository, releases, channel, internal)
    checksums(internal)


def projected_history(releases, tag, nightly):
    history = [release for release in releases if not release["draft"] and release["tag_name"] != tag]
    if nightly:
        retained = sorted((release for release in history if release["prerelease"] and
                           packages.NIGHTLY.fullmatch(release["tag_name"])),
                          key=lambda release: (release["published_at"], release["tag_name"]), reverse=True)
        retained = retained[:packages.NIGHTLY_RETENTION - 1]
        keep = {release["tag_name"] for release in retained}
        history = [release for release in history if not release["prerelease"] or release["tag_name"] in keep]
    return history


def bridge_window(history, channel):
    visible = history[:packages.SDK_RELEASE_WINDOW - 1]
    if channel.endswith("-releases"):
        return [release for release in visible if not release["prerelease"]][:packages.STABLE_BRIDGE_LOOKBACK]
    return visible


def bridge(main_releases, internal, downloads, nightly):
    """Keep migration feeds discoverable without adding them to every ordinary release."""
    for feed in internal.glob("releases.*.json"):
        channel = feed.name[len("releases."):-len(".json")]
        release_track = channel.endswith("-releases")
        if release_track and nightly:
            continue
        window = bridge_window(main_releases, channel)
        if any(MARKER in (release.get("body") or "") and feed.name in {item["name"] for item in release["assets"]}
               for release in window):
            continue
        fulls = [asset for asset in packages.feed_assets(json.loads(feed.read_text()), channel) if asset["Type"] == "Full"]
        latest = max(fulls, key=lambda asset: packages.precedence(asset["Version"]))
        # Migration always has a full fallback, even when a client skipped every intermediate delta.
        packages.verify(internal / latest["FileName"], latest)
        shutil.copy2(internal / latest["FileName"], downloads / latest["FileName"])
        (downloads / feed.name).write_text(json.dumps({"Assets": [latest]}) + "\n")
    checksums(downloads)


def candidate_window(history, tag, nightly):
    retained = {release["tag_name"] for release in projected_history(history, tag, nightly)}
    window = [release for release in history if release["tag_name"] == tag or release["tag_name"] in retained]
    window = window[:packages.SDK_RELEASE_WINDOW]
    if tag not in {release["tag_name"] for release in window}:
        raise ValueError("The tagged commit is outside the SDK discovery window; use a recent commit")
    return window


def download_notes(notes, downloads, version):
    lines = [notes.rstrip(), "", "Choose the file for your system:", ""]
    for platform in packages.INSTALLERS:
        name = installer_name(platform, version)
        if (downloads / name).exists():
            lines.append(f"- `{name}`")
    if (downloads / installer_name("linux-arm64-glibc239", version)).exists() and ARM64_REQUIREMENTS not in notes:
        lines += ["", ARM64_REQUIREMENTS]
    if list(downloads.glob("releases.*.json")):
        lines += ["", MARKER, "The additional update packages keep older Perikop installations working. "
                  "For a new installation, choose one of the files above."]
    source_url = f"https://github.com/{packages.UPDATE_REPOSITORY}/releases"
    lines += ["", "<details><summary>Runtime source and verification</summary>", "",
              f"[AppImage runtime corresponding source and internal checksums]({source_url}). "
              "`SHA256SUMS.txt` in this release covers its downloads and compatibility assets.", "", "</details>"]
    return "\n".join(lines) + "\n"


def validate_repository(releases):
    for release in releases:
        if not release["draft"] and not (release["tag_name"].startswith("v") or packages.NIGHTLY.fullmatch(release["tag_name"])):
            raise ValueError("Only application releases belong in the update repository: " + release["tag_name"])


def stage(source, main, updates, output, version, tag, notes, target, nightly):
    if packages.precedence(version) < packages.precedence("0.2.1-0") or tag == "v0.2.0":
        raise ValueError("Migration publication starts at 0.2.1")
    if nightly != bool(packages.NIGHTLY.fullmatch(tag)) or (not nightly and tag != "v" + version):
        raise ValueError("Release tag, track and version disagree")
    feeds = list(source.glob("releases.*.json"))
    if any(not feed.name.endswith("-nightly.json" if nightly else "-releases.json") for feed in feeds):
        raise ValueError("Build feeds do not match the publication track")
    downloads, internal = split(source, output, version, not nightly)
    history = published(updates)
    validate_repository(history)
    carry_feeds(updates, history, internal)
    main_history = main.releases()
    validate_repository(main_history)
    bridge(projected_history(main_history, tag, nightly), internal, downloads, nightly)
    main_notes = output / "download-notes.md"
    main_notes.write_text(download_notes(notes.read_text(), downloads, version))
    update_notes = output / "update-notes.md"
    update_notes.write_text(f"Internal update assets for [{tag}](https://github.com/{main.name}/releases/tag/{tag}).\n")
    updates.draft(tag, internal, update_notes, nightly, target)
    main.draft(tag, downloads, main_notes, nightly, target)


def trim_nightlies(repository, tag):
    nightlies = [release for release in published(repository)
                if release["prerelease"] and packages.NIGHTLY.fullmatch(release["tag_name"])]
    keep = packages.NIGHTLY_RETENTION
    if not any(release["tag_name"] == tag for release in nightlies):
        keep -= 1
    if len(nightlies) > keep:
        current = max(nightlies, key=lambda release: (release["published_at"], release["tag_name"]))["tag_name"]
        packages.cleanup(repository.name, current, keep, token=repository.token)


def promote(main, updates, tag, nightly):
    if tag == "v0.2.0" or (not nightly and (not tag.startswith("v") or
                                          packages.precedence(tag[1:]) < packages.precedence("0.2.1"))):
        raise ValueError("Migration publication starts at 0.2.1")
    if nightly != bool(packages.NIGHTLY.fullmatch(tag)):
        raise ValueError("Release tag and track disagree")
    # Both repositories are verified again after human review and before either draft becomes public.
    internal = updates.verify(tag)
    downloads = main.verify(tag)
    versions = set()
    platforms = set()
    for platform, suffix in packages.INSTALLERS.items():
        prefix = f"Perikop-{platform}-"
        names = [name for name in downloads if name.startswith(prefix) and name.endswith(suffix)]
        if len(names) > 1:
            raise ValueError("Duplicate primary installer: " + platform)
        if names:
            version = names[0][len(prefix):-len(suffix)]
            packages.precedence(version)
            versions.add(version)
            platforms.add(platform)
            channel = platform + ("-nightly" if nightly else "-releases")
            if f"releases.{channel}.json" not in internal or f"Perikop-{version}-{channel}-full.nupkg" not in internal:
                raise ValueError("Primary download has no update feed or full package: " + platform)
    if any(platform.startswith("linux-") for platform in platforms) and not any(
            name.startswith("AppImage-runtime-source-") for name in internal):
        raise ValueError("AppImage publication is missing its runtime source")
    if len(versions) != 1 or (not nightly and (platforms != set(packages.INSTALLERS) or versions != {tag[1:]})):
        raise ValueError("Primary downloads disagree with the release version or platform coverage")
    main_history, update_history = main.releases(), updates.releases()
    validate_repository(main_history)
    validate_repository(update_history)
    for history in (main_history, update_history):
        candidate = next(release for release in history if release["tag_name"] == tag)
        if candidate["prerelease"] != nightly:
            raise ValueError("Remote release track disagrees with its tag")
        candidate_window(history, tag, nightly)
    candidate = next(release for release in main_history if release["tag_name"] == tag)
    if any(name.startswith("releases.") for name in downloads) and MARKER not in (candidate.get("body") or ""):
        raise ValueError("Compatibility release notes lost their migration marker")
    history = projected_history(main_history, tag, nightly)
    for name in internal:
        if not name.startswith("releases.") or (nightly and name.endswith("-releases.json")):
            continue
        channel = name[len("releases."):-len(".json")]
        window = bridge_window(history, channel)
        if name not in downloads and not any(MARKER in (release.get("body") or "") and name in {
                item["name"] for item in release["assets"]} for release in window):
            raise ValueError("Migration bridge aged out during draft review; restage before publishing: " + name)
    # Prune before promotion so even the temporary SDK window contains at most seven nightlies.
    if nightly:
        trim_nightlies(updates, tag)
        trim_nightlies(main, tag)
    updates.promote(tag, nightly)
    main.promote(tag, nightly)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    staging = commands.add_parser("stage")
    staging.add_argument("--source", type=Path, required=True)
    staging.add_argument("--output", type=Path, required=True)
    staging.add_argument("--version", required=True)
    staging.add_argument("--notes", type=Path, required=True)
    staging.add_argument("--target", required=True)
    promoting = commands.add_parser("promote")
    for command in (staging, promoting):
        command.add_argument("--tag", required=True)
        command.add_argument("--nightly", action="store_true")
    args = parser.parse_args()
    main_repo = Repository(packages.LEGACY_REPOSITORY, os.environ.get("GH_TOKEN"))
    update_repo = Repository(packages.UPDATE_REPOSITORY, os.environ.get("UPDATES_TOKEN"))
    if args.command == "stage":
        stage(args.source, main_repo, update_repo, args.output, args.version, args.tag, args.notes, args.target, args.nightly)
    else:
        promote(main_repo, update_repo, args.tag, args.nightly)


if __name__ == "__main__":
    main()
