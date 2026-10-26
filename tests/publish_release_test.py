"""Paired publication and the pinned SDK's ten-release discovery window, using local bytes only."""

import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/ci"))
import publish_release as publication
import update_packages as packages


class GitHubFixture:
    def __init__(self):
        self.repositories = {packages.LEGACY_REPOSITORY: [], packages.UPDATE_REPOSITORY: []}
        self.calls = []
        self.fail = None
        self.sequence = 0

    def release(self, repository, tag):
        return next(item for item in self.repositories[repository] if item["tag_name"] == tag)

    def gh(self, *arguments, token=None):
        args = list(map(str, arguments))
        self.calls.append((token if token is not None else os.environ.get("GH_TOKEN"), args))
        if self.fail and self.fail(args):
            raise RuntimeError("Injected publication failure")
        if args[0] == "api":
            if "--method" in args:
                return json.dumps({"sha": "b" * 40})
            if args[-1].endswith("/commits/main"):
                return json.dumps({"sha": "c" * 40, "commit": {"tree": {"sha": "d" * 40}}})
            repository = args[-1][len("repos/"):].split("/releases")[0]
            return json.dumps([[{key: value for key, value in item.items() if key != "files"}
                                for item in self.repositories[repository]]])
        command, tag = args[1:3]
        repository = args[args.index("--repo") + 1]
        if command == "create":
            self.sequence += 1
            self.repositories[repository].insert(0, {
                "tag_name": tag, "draft": True, "prerelease": "--prerelease" in args,
                "body": Path(args[args.index("--notes-file") + 1]).read_text(), "assets": [], "files": {},
                "published_at": f"2026-10-25T00:{self.sequence:02d}:00Z"})
            return ""
        release = self.release(repository, tag)
        if command == "upload":
            paths = args[3:args.index("--repo")] + args[args.index("--clobber") + 1:] if "--clobber" in args else [args[3]]
            for name in paths:
                path = Path(name)
                release["files"][path.name] = path.read_bytes()
            release["assets"] = [{"name": name} for name in release["files"]]
        elif command == "download":
            directory = Path(args[args.index("--dir") + 1])
            files = release["files"]
            if "--pattern" in args:
                name = args[args.index("--pattern") + 1]
                files = {name: files[name]}
            for name, data in files.items():
                (directory / name).write_bytes(data)
        elif command == "edit":
            if "--draft=false" in args:
                release["draft"] = False
            if "--notes-file" in args:
                release["body"] = Path(args[args.index("--notes-file") + 1]).read_text()
        elif command == "view":
            return json.dumps({"assets": release["assets"]})
        elif command == "delete":
            self.repositories[repository].remove(release)
        else:
            raise AssertionError(args)
        return ""

    def sdk_feed(self, repository, channel, nightly=False):
        # GithubSource::get_releases takes ten BEFORE excluding prereleases; then merges matching feeds.
        visible = [release for release in self.repositories[repository] if not release["draft"]][:10]
        return [asset for release in visible if nightly or not release["prerelease"]
                for asset in json.loads(release["files"].get(f"releases.{channel}.json", b'{"Assets": []}'))["Assets"]]

    def sdk_download(self, repository, entry, nightly=False):
        visible = [release for release in self.repositories[repository] if not release["draft"]][:10]
        data = next(release["files"][entry["FileName"]] for release in visible
                    if (nightly or not release["prerelease"]) and entry["FileName"] in release["files"])
        assert len(data) == entry["Size"] and hashlib.sha256(data).hexdigest() == entry["SHA256"]


def build(directory, version, track="releases", platforms=None, base=None):
    directory.mkdir()
    for platform in platforms or packages.INSTALLERS:
        channel = platform + "-" + track
        entries = []
        for number, kind in ([(base, "Full")] if base else []) + [(version, "Full")] + ([(version, "Delta")] if base else []):
            name = f"Perikop-{number}-{channel}-{kind.lower()}.nupkg"
            data = (name + " payload").encode()
            entries.append({"PackageId": "Perikop", "Version": number, "Type": kind, "FileName": name,
                            "SHA256": hashlib.sha256(data).hexdigest(), "Size": len(data)})
            (directory / name).write_bytes(data)
        (directory / f"releases.{channel}.json").write_text(json.dumps({"Assets": entries}))
        (directory / f"Perikop-{channel}{packages.INSTALLERS[platform]}").write_bytes(b"installer " + version.encode())
        if platform.startswith("linux-"):
            (directory / "AppImage-runtime-source-3d17002.tar.gz").write_bytes(b"runtime source")


class CredentialTest(unittest.TestCase):
    def test_subprocess_gets_scoped_token_without_changing_parent_even_after_failure(self):
        for fail in (False, True):
            with self.subTest(fail=fail), patch.dict(os.environ, GH_TOKEN="parent-token"), \
                    patch.object(packages.subprocess, "check_output", side_effect=RuntimeError("gh failed") if fail else None,
                                 return_value="ok") as run:
                if fail:
                    with self.assertRaisesRegex(RuntimeError, "gh failed"):
                        packages.gh("api", "repos/owner/repo", token="scoped-token")
                else:
                    self.assertEqual(packages.gh("api", "repos/owner/repo", token="scoped-token"), "ok")
                self.assertEqual(run.call_args.kwargs["env"]["GH_TOKEN"], "scoped-token")
                self.assertEqual(os.environ["GH_TOKEN"], "parent-token")


class PublicationTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.server = GitHubFixture()
        self.patcher = patch.object(packages, "gh", side_effect=self.server.gh)
        self.patcher.start()
        self.addCleanup(self.patcher.stop)
        self.main = publication.Repository(packages.LEGACY_REPOSITORY, "main-token")
        self.updates = publication.Repository(packages.UPDATE_REPOSITORY, "updates-token")
        self.counter = 0

    def stage(self, version="0.2.1", nightly=False, platforms=None, base=None):
        self.counter += 1
        source = self.root / f"build-{self.counter}"
        build(source, version, "nightly" if nightly else "releases", platforms, base)
        notes = self.root / "notes.md"
        notes.write_text("Perikop notes\n")
        tag = f"nightly-2026-10-25-aaaaaaa-{self.counter}" if nightly else "v" + version
        publication.stage(source, self.main, self.updates, self.root / f"staged-{self.counter}",
                          version, tag, notes, "a" * 40, nightly)
        return tag

    def test_old_source_migrates_then_new_source_finds_next_version_on_every_platform(self):
        legacy = self.root / "legacy"
        build(legacy, "0.2.0")
        publication.checksums(legacy)
        original = {file.name: file.read_bytes() for file in legacy.iterdir()}
        self.server.repositories[self.main.name].append({
            "tag_name": "v0.2.0", "draft": False, "prerelease": False, "body": "Original release",
            "published_at": "2026-10-25T00:00:00Z", "files": dict(original),
            "assets": [{"name": name} for name in original]})
        tag = self.stage(base="0.2.0")
        self.assertTrue(self.server.release(self.main.name, tag)["draft"])
        self.assertTrue(self.server.release(self.updates.name, tag)["draft"])
        notes = self.server.release(self.main.name, tag)["body"]
        self.assertIn("Raspberry Pi 3, 4 and 5", notes)
        self.assertIn("64-bit Raspberry Pi OS Trixie", notes)
        self.assertIn("sudo apt install libespeak-ng1", notes)
        self.assertIn("Pi 3/4 compatibility is expected", notes)
        publication.promote(self.main, self.updates, tag, False)
        for platform in packages.INSTALLERS:
            feed = self.server.sdk_feed(self.main.name, platform + "-releases")
            target = max(feed, key=lambda item: packages.precedence(item["Version"]))
            self.assertEqual(target["Version"], "0.2.1")
            self.assertEqual(target["Type"], "Full")
            self.server.sdk_download(self.main.name, target)
        next_tag = self.stage("0.2.2", base="0.2.1")
        publication.promote(self.main, self.updates, next_tag, False)
        for platform in packages.INSTALLERS:
            feed = self.server.sdk_feed(self.updates.name, platform + "-releases")
            targets = [item for item in feed if item["Version"] == "0.2.2"]
            self.assertEqual({item["Type"] for item in targets}, {"Full", "Delta"})
            for item in targets:
                self.server.sdk_download(self.updates.name, item)
        names = set(self.server.release(self.main.name, next_tag)["files"])
        self.assertEqual(names, {publication.installer_name(platform, "0.2.2") for platform in packages.INSTALLERS}
                         | {"SHA256SUMS.txt"})
        self.assertNotIn("AppImage-runtime-source-3d17002.tar.gz", names)
        self.assertIn('perikop-updates', (ROOT / 'src/app/updates.cpp').read_text())
        self.assertEqual(self.server.release(self.main.name, "v0.2.0")["files"], original)

    def test_nightly_migration_and_partial_build_preserve_other_platforms_and_tracks(self):
        stable = self.stage()
        publication.promote(self.main, self.updates, stable, False)
        nightly = self.stage("0.2.2-nightly.20261025.1.1", nightly=True)
        publication.promote(self.main, self.updates, nightly, True)
        partial = self.stage("0.2.2-nightly.20261025.2.1", nightly=True, platforms=["win-x64"])
        publication.promote(self.main, self.updates, partial, True)
        for platform in packages.INSTALLERS:
            old_feed = self.server.sdk_feed(self.main.name, platform + "-nightly", nightly=True)
            target = max(old_feed, key=lambda item: packages.precedence(item["Version"]))
            self.assertEqual(target["Version"], "0.2.2-nightly.20261025.1.1")
            self.server.sdk_download(self.main.name, target, nightly=True)
            feed = self.server.sdk_feed(self.updates.name, platform + "-nightly", nightly=True)
            target = max(feed, key=lambda item: packages.precedence(item["Version"]))
            self.assertEqual(target["Version"], "0.2.2-nightly.20261025." + ("2.1" if platform == "win-x64" else "1.1"))
            self.server.sdk_download(self.updates.name, target, nightly=True)
            stable_feed = self.server.sdk_feed(self.updates.name, platform + "-releases")
            self.assertTrue(stable_feed)
            self.assertTrue(all(item["Version"] == "0.2.1" for item in stable_feed))
            self.assertFalse(self.server.sdk_feed(self.updates.name, platform.replace("-glibc239", "") + "-nightly", True)
                             if platform.startswith("linux-") else False)
        internal = self.server.release(self.updates.name, partial)["files"]
        self.assertIn("AppImage-runtime-source-3d17002.tar.gz", internal)

    def test_bridge_refreshes_before_old_clients_lose_it(self):
        for patch_number in range(1, 14):
            version = f"0.2.{patch_number}"
            tag = self.stage(version)
            publication.promote(self.main, self.updates, tag, False)
            for platform in packages.INSTALLERS:
                feed = self.server.sdk_feed(self.main.name, platform + "-releases")
                target = max(feed, key=lambda item: packages.precedence(item["Version"]))
                self.assertGreaterEqual(packages.precedence(target["Version"]), packages.precedence("0.2.1"))
                self.server.sdk_download(self.main.name, target)
            has_bridge = any(name.startswith("releases.") for name in self.server.release(self.main.name, tag)["files"])
            self.assertEqual(has_bridge, patch_number % 3 == 1)

    def test_retention_and_mixed_publications_preserve_every_migration_feed(self):
        tag = self.stage()
        publication.promote(self.main, self.updates, tag, False)
        for number in range(1, 11):
            tag = self.stage(f"0.2.2-nightly.20261025.{number}.1", nightly=True,
                             platforms=None if number == 1 else ["win-x64"])
            publication.promote(self.main, self.updates, tag, True)
            for repository in (self.main, self.updates):
                self.assertLessEqual(sum(not item["draft"] and item["prerelease"]
                                         for item in self.server.repositories[repository.name]), 7)
            for platform in packages.INSTALLERS:
                for repository in (self.main, self.updates):
                    feed = self.server.sdk_feed(repository.name, platform + "-nightly", True)
                    target = max(feed, key=lambda item: packages.precedence(item["Version"]))
                    self.server.sdk_download(repository.name, target, True)
        for number in range(2, 14):
            tag = self.stage(f"0.2.{number}")
            publication.promote(self.main, self.updates, tag, False)
            for platform in packages.INSTALLERS:
                for repository in (self.main, self.updates):
                    for track in ("releases", "nightly"):
                        feed = self.server.sdk_feed(repository.name, platform + "-" + track, track == "nightly")
                        target = max(feed, key=lambda item: packages.precedence(item["Version"]))
                        self.server.sdk_download(repository.name, target, track == "nightly")

    def test_remote_missing_feed_with_rewritten_checksums_still_cannot_promote(self):
        tag = self.stage()
        release = self.server.release(self.updates.name, tag)
        del release["files"]["releases.win-x64-releases.json"]
        release["files"]["SHA256SUMS.txt"] = packages.checksum_text({
            name: hashlib.sha256(data).hexdigest() for name, data in release["files"].items()
            if name != "SHA256SUMS.txt"}).encode()
        with self.assertRaisesRegex(ValueError, "not referenced"):
            publication.promote(self.main, self.updates, tag, False)
        self.assertTrue(self.server.release(self.main.name, tag)["draft"])
        self.assertTrue(release["draft"])

    def test_empty_feed_with_valid_checksums_cannot_hide_the_target_package(self):
        tag = self.stage()
        release = self.server.release(self.updates.name, tag)
        release["files"]["releases.win-x64-releases.json"] = b'{"Assets": []}'
        release["files"]["SHA256SUMS.txt"] = packages.checksum_text({
            name: hashlib.sha256(data).hexdigest() for name, data in release["files"].items()
            if name != "SHA256SUMS.txt"}).encode()
        with self.assertRaisesRegex(ValueError, "not referenced"):
            publication.promote(self.main, self.updates, tag, False)
        self.assertTrue(self.server.release(self.main.name, tag)["draft"])

    def test_source_missing_from_older_checksum_manifest_blocks_staging_with_clear_error(self):
        tag = self.stage()
        publication.promote(self.main, self.updates, tag, False)
        release = self.server.release(self.updates.name, tag)
        source = "AppImage-runtime-source-3d17002.tar.gz"
        manifest = packages.read_checksums(release["files"]["SHA256SUMS.txt"].decode())
        del manifest[source]
        release["files"]["SHA256SUMS.txt"] = packages.checksum_text(manifest).encode()
        self.server.calls.clear()
        with self.assertRaisesRegex(ValueError, "Runtime source missing from checksum manifest: " + source):
            self.stage("0.2.2-nightly.20261025.1.1", nightly=True, platforms=["win-x64"])
        self.assertFalse(any(args[:2] == ["release", "create"] for _, args in self.server.calls))
        self.assertFalse(any(args[:2] == ["release", "delete"] for _, args in self.server.calls))

    def test_corrupt_source_snapshot_cannot_be_carried_into_a_partial_nightly(self):
        tag = self.stage()
        publication.promote(self.main, self.updates, tag, False)
        release = self.server.release(self.updates.name, tag)
        release["files"]["AppImage-runtime-source-3d17002.tar.gz"] = b"corrupt source"
        with self.assertRaisesRegex(ValueError, "Runtime source checksum mismatch"):
            self.stage("0.2.2-nightly.20261025.1.1", nightly=True, platforms=["win-x64"])

    def test_update_upload_or_download_failure_never_publishes_main(self):
        for command in ("upload", "download"):
            with self.subTest(command=command):
                self.server.fail = lambda args: args[:2] == ["release", command] and self.updates.name in args
                with self.assertRaisesRegex(RuntimeError, "Injected"):
                    self.stage()
                self.assertFalse(any(not item["draft"] for item in self.server.repositories[self.main.name]))
                self.server.fail = None

    def test_remote_corruption_stops_promotion_before_either_repository_is_public(self):
        tag = self.stage()
        release = self.server.release(self.updates.name, tag)
        name = next(name for name in release["files"] if name.endswith("-full.nupkg"))
        release["files"][name] = b"corrupt"
        with self.assertRaisesRegex(ValueError, "checksum mismatch"):
            publication.promote(self.main, self.updates, tag, False)
        self.assertTrue(release["draft"])
        self.assertTrue(self.server.release(self.main.name, tag)["draft"])

    def test_promotion_failure_keeps_main_draft_and_retry_finishes_safely(self):
        tag = self.stage()
        self.server.fail = lambda args: args[:2] == ["release", "edit"] and self.main.name in args and "--draft=false" in args
        with self.assertRaisesRegex(RuntimeError, "Injected"):
            publication.promote(self.main, self.updates, tag, False)
        self.assertFalse(self.server.release(self.updates.name, tag)["draft"])
        self.assertTrue(self.server.release(self.main.name, tag)["draft"])
        self.server.fail = None
        publication.promote(self.main, self.updates, tag, False)
        self.assertFalse(self.server.release(self.main.name, tag)["draft"])

    def test_scoped_tokens_are_used_for_their_own_repositories(self):
        self.stage()
        for token, args in self.server.calls:
            if "--repo" in args:
                repository = args[args.index("--repo") + 1]
                self.assertEqual(token, "main-token" if repository == self.main.name else "updates-token")
            elif args[0] == "api":
                endpoint = next(value for value in args if value.startswith("repos/"))
                self.assertEqual(token, "main-token" if endpoint.startswith("repos/" + self.main.name + "/")
                                 else "updates-token")

    def test_dependency_snapshots_are_rejected_from_update_storage(self):
        self.server.repositories[self.updates.name].append({"draft": False, "tag_name": "ci-dependencies-aaaaaaa"})
        with self.assertRaisesRegex(ValueError, "Only application releases"):
            self.stage()
        self.assertFalse(any(args[:2] == ["release", "create"] for _, args in self.server.calls))

    def test_missing_feed_package_checksum_or_installer_cannot_create_drafts(self):
        for pattern in ("releases.win*.json", "*-full.nupkg", "*-Setup.pkg", "AppImage-runtime-source-*"):
            with self.subTest(pattern=pattern):
                self.counter += 1
                source = self.root / f"missing-{self.counter}"
                build(source, "0.2.1")
                next(source.glob(pattern)).unlink()
                with self.assertRaises((ValueError, FileNotFoundError)):
                    publication.split(source, self.root / f"refused-{self.counter}", "0.2.1", True)
        self.assertFalse(self.server.calls)

    def test_candidate_outside_api_window_cannot_be_promoted(self):
        tag = self.stage()
        releases = self.server.repositories[self.main.name]
        candidate = releases.pop()
        for number in range(10):
            releases.append({"tag_name": f"v0.1.{number}", "draft": False, "prerelease": False,
                             "body": "", "assets": [], "files": {}})
        releases.append(candidate)
        with self.assertRaisesRegex(ValueError, "outside the SDK discovery window"):
            publication.promote(self.main, self.updates, tag, False)
        self.assertTrue(candidate["draft"])
        self.assertTrue(self.server.release(self.updates.name, tag)["draft"])

    def test_live_020_release_cannot_be_promoted_by_migration_tool(self):
        with self.assertRaisesRegex(ValueError, "starts at 0.2.1"):
            publication.promote(self.main, self.updates, "v0.2.0", False)
        self.assertFalse(self.server.calls)


if __name__ == "__main__":
    unittest.main()
