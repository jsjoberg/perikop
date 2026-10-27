"""Update publications remain verifiable, and the updates release never loses a package a feed names."""

import hashlib
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("update_packages", ROOT / "tools/ci/update_packages.py")
updates = importlib.util.module_from_spec(spec)
spec.loader.exec_module(updates)
CHANNEL = "osx-arm64-nightly"
windows_spec = importlib.util.spec_from_file_location("windows_update_test", ROOT / "tools/ci/velopack_windows_test.py")
windows = importlib.util.module_from_spec(windows_spec)
windows_spec.loader.exec_module(windows)


def asset(version, kind="Full", channel=CHANNEL):
    data = (version + kind).encode()
    return {"PackageId": "Perikop", "Version": version, "Type": kind,
            "FileName": f"Perikop-{version}-{channel}-{kind.lower()}.nupkg",
            "SHA256": hashlib.sha256(data).hexdigest(), "Size": len(data)}


def write_asset(directory, entry):
    path = directory / entry["FileName"]
    path.write_bytes((entry["Version"] + entry["Type"]).encode())
    return path


class UpdatePackagesTest(unittest.TestCase):
    def fixture(self, directory, with_base=True):
        full, delta, base = asset("1.0.0-test.2"), asset("1.0.0-test.2", "Delta"), asset("1.0.0-test.1")
        published = [full, delta] if with_base else [full]
        entries = [base] + published if with_base else published
        for entry in entries:
            write_asset(directory, entry)
        (directory / f"releases.{CHANNEL}.json").write_text(json.dumps({"Assets": entries}))
        (directory / f"assets.{CHANNEL}.json").write_text(json.dumps(
            [{"RelativeFileName": entry["FileName"], "Type": entry["Type"]} for entry in published]))
        return entries

    def test_publication_has_new_full_and_delta_but_does_not_duplicate_the_base(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            entries = self.fixture(source)
            destination = source / "publication"
            updates.finish(source, CHANNEL, "1.0.0-test.2", destination)
            self.assertFalse((destination / entries[0]["FileName"]).exists())
            self.assertTrue((destination / entries[1]["FileName"]).is_file())
            self.assertTrue((destination / entries[2]["FileName"]).is_file())
            feed = json.loads((destination / f"releases.{CHANNEL}.json").read_text())
            self.assertEqual(feed["Assets"], entries)

    def test_first_publication_needs_no_delta(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            self.fixture(source, with_base=False)
            updates.finish(source, CHANNEL, "1.0.0-test.2", source / "publication")

    def test_new_publication_rejects_retired_targets(self):
        for platform in ("osx-x64", "linux-x64", "linux-arm64", "linux-x64-glibc241", "linux-arm64-glibc241"):
            with self.subTest(platform=platform), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                publication = root / "updates"
                publication.mkdir()
                channel = platform + "-nightly"
                entry = asset("1.0.0", channel=channel)
                write_asset(publication, entry)
                (publication / f"releases.{channel}.json").write_text(json.dumps({"Assets": [entry]}))
                with self.assertRaisesRegex(ValueError, "retired platform"):
                    updates.assemble(publication, root / "release", "1.0.0")
                self.assertFalse((root / "release").exists())

    def test_first_debian_build_ignores_earlier_linux_delta_bases(self):
        for platform, architecture in (("Linux", "x64"), ("Linux-arm64", "arm64")):
            for nightly, track in (("false", "releases"), ("true", "nightly")):
                with self.subTest(platform=platform, track=track), tempfile.TemporaryDirectory() as temporary, \
                        patch.dict(os.environ, NIGHTLY=nightly):
                    old_channel = f"linux-{architecture}-{track}"
                    old = asset("1.0.0", channel=old_channel)
                    releases = [{"tag_name": "old", "prerelease": nightly == "true",
                                 "assets": [{"name": f"releases.{old_channel}.json"}, {"name": old["FileName"]}]}]
                    with patch.object(updates, "releases", return_value=releases), \
                            patch.object(updates, "download") as download:
                        updates.restore("owner/repo", platform, Path(temporary), "1.0.1")
                    channel = f"linux-{architecture}-glibc239-{track}"
                    self.assertEqual(updates.channel_for(platform), channel)
                    self.assertEqual(list((Path(temporary) / channel).iterdir()), [])
                    download.assert_not_called()

    def test_existing_base_requires_a_delta(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            entries = self.fixture(source)
            (source / f"releases.{CHANNEL}.json").write_text(json.dumps({"Assets": entries[:2]}))
            with self.assertRaisesRegex(ValueError, "no delta"):
                updates.finish(source, CHANNEL, "1.0.0-test.2", source / "publication")
            self.assertFalse((source / "publication").exists())

    def test_corrupt_base_stops_publication(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            entries = self.fixture(source)
            (source / entries[0]["FileName"]).write_bytes(b"corrupt")
            with self.assertRaisesRegex(ValueError, "size"):
                updates.finish(source, CHANNEL, "1.0.0-test.2", source / "publication")
            self.assertFalse((source / "publication").exists())

    def test_historical_deltas_are_not_repeated_in_the_new_feed(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary)
            entries = self.fixture(source)
            (source / f"releases.{CHANNEL}.json").write_text(json.dumps({"Assets": entries + [asset("1.0.0-test.1", "Delta")]}))
            destination = source / "publication"
            updates.finish(source, CHANNEL, "1.0.0-test.2", destination)
            feed = json.loads((destination / f"releases.{CHANNEL}.json").read_text())
            self.assertEqual([entry["Version"] for entry in feed["Assets"] if entry["Type"] == "Delta"], ["1.0.0-test.2"])

    def test_feed_rejects_wrong_channel_path_traversal_and_bad_hashes(self):
        for field, value in (("FileName", "../../secret"), ("FileName", "Perikop-1.0.0-linux-x64-nightly-full.nupkg"),
                             ("SHA256", "invalid"), ("PackageId", "AnotherApp"), ("Size", -1)):
            with self.subTest(field=field, value=value):
                entry = asset("1.0.0")
                entry[field] = value
                with self.assertRaises(ValueError):
                    updates.feed_assets({"Assets": [entry]}, CHANNEL)

    def test_assemble_gives_downloads_plain_names_and_keeps_packages_apart(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            publication = root / "updates"
            self.fixture(root, with_base=False)
            updates.finish(root, CHANNEL, "1.0.0-test.2", publication)
            # Each feed ships beside the installer people download.
            with self.assertRaisesRegex(ValueError, "lacks its installer"):
                updates.assemble(publication, root / "refused", "1.0.0-test.2")
            (publication / f"Perikop-{CHANNEL}-Setup.pkg").write_bytes(b"installer")
            (publication / f"Perikop-{CHANNEL}-Portable.zip").write_bytes(b"portable")
            output = root / "release"
            updates.assemble(publication, output, "1.0.0-test.2")
            downloads = output / "downloads"
            self.assertEqual(sorted(path.name for path in downloads.iterdir()),
                             ["Perikop-macOS-Portable.zip", "Perikop.pkg", "SHA256SUMS.txt"])
            self.assertEqual(sorted(path.name for path in (output / "packages").iterdir()),
                             [asset("1.0.0-test.2")["FileName"], f"releases.{CHANNEL}.json"])
            lines = (downloads / "SHA256SUMS.txt").read_text().splitlines()
            self.assertEqual(len(lines), 2)
            for line in lines:
                digest, name = line.split("  ")
                self.assertEqual(digest, hashlib.sha256((downloads / name).read_bytes()).hexdigest())
            (publication / "notes.txt").write_text("stray")
            with self.assertRaisesRegex(ValueError, "Unexpected release file"):
                updates.assemble(publication, root / "stray", "1.0.0-test.2")

    def test_only_0_2_1_carries_packages_for_0_2_0_copies(self):
        channel = "win-x64-releases"
        for version, bridged in (("0.2.1", True), ("0.2.2", False)):
            with self.subTest(version=version), tempfile.TemporaryDirectory() as temporary:
                publication = Path(temporary) / "updates"
                publication.mkdir()
                full = asset(version, channel=channel)
                write_asset(publication, full)
                (publication / f"releases.{channel}.json").write_text(json.dumps({"Assets": [full]}))
                (publication / f"Perikop-{channel}-Setup.exe").write_bytes(b"installer")
                updates.assemble(publication, Path(temporary) / "release", version)
                downloads = {path.name for path in (Path(temporary) / "release/downloads").iterdir()}
                self.assertIn("Perikop-Setup.exe", downloads)
                self.assertEqual(f"releases.{channel}.json" in downloads, bridged)
                self.assertEqual(full["FileName"] in downloads, bridged)

    def test_assemble_ships_the_appimage_runtime_source_beside_appimages(self):
        channel = "linux-x64-glibc239-nightly"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            publication = root / "updates"
            publication.mkdir()
            (publication / f"Perikop-{channel}.AppImage").write_bytes(b"appimage")
            full = asset("1.0.0-test.2", channel=channel)
            write_asset(publication, full)
            (publication / f"releases.{channel}.json").write_text(json.dumps({"Assets": [full]}))
            # The runtime statically links libfuse (LGPL-2.1), so AppImages never ship without its source.
            with self.assertRaisesRegex(ValueError, "corresponding source"):
                updates.assemble(publication, root / "refused", "1.0.0-test.2")
            (publication / "AppImage-runtime-source-3d17002.tar.gz").write_bytes(b"source")
            updates.assemble(publication, root / "release", "1.0.0-test.2")
            self.assertTrue((root / "release/downloads/Perikop-x86_64.AppImage").is_file())
            checksums = (root / "release/downloads/SHA256SUMS.txt").read_text()
            self.assertIn(hashlib.sha256(b"source").hexdigest() + "  AppImage-runtime-source-3d17002.tar.gz\n",
                          checksums)

    def test_assemble_refuses_a_missing_platform_or_mismatched_version(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.fixture(root, with_base=False)
            updates.finish(root, CHANNEL, "1.0.0-test.2", root / "updates")
            (root / "updates" / f"Perikop-{CHANNEL}-Setup.pkg").write_bytes(b"installer")
            with self.assertRaisesRegex(ValueError, "does not match"):
                updates.assemble(root / "updates", root / "release", "1.0.0-test.3")
            # A nightly may lack platforms; a release needs all four.
            with self.assertRaisesRegex(ValueError, "linux-arm64-glibc239, linux-x64-glibc239, win-x64"):
                updates.assemble(root / "updates", root / "release", "1.0.0-test.2", complete=True)
            self.assertFalse((root / "release").exists())

    def test_restore_chooses_a_verified_older_base_and_never_fetches_the_target(self):
        versions = ["1.0.0-test.1", "1.0.0-test.2", "1.0.0-test.3"]
        feeds = {"release-" + version: {"Assets": [asset(version)]} for version in versions}
        releases = [{"tag_name": tag, "draft": False, "prerelease": True,
                     "assets": [{"name": f"releases.{CHANNEL}.json"}, {"name": feed["Assets"][0]["FileName"]}]}
                    for tag, feed in feeds.items()]
        downloaded = []
        def download(repository, tag, name, directory):
            downloaded.append(name)
            path = directory / name
            if name.endswith(".json"):
                path.write_text(json.dumps(feeds[tag]))
            else:
                write_asset(directory, feeds[tag]["Assets"][0])
            return path
        with tempfile.TemporaryDirectory() as temporary, patch.dict(os.environ, NIGHTLY="true"), \
                patch.object(updates, "releases", return_value=releases), patch.object(updates, "download", side_effect=download):
            output = Path(temporary)
            updates.restore("owner/repo", "macOS-arm64", output, "1.0.0-test.3")
            packages = list((output / CHANNEL).glob("*.nupkg"))
            self.assertEqual([path.name for path in packages], [asset("1.0.0-test.2")["FileName"]])
            self.assertNotIn(asset("1.0.0-test.3")["FileName"], downloaded)

    def test_restore_prefers_the_updates_release_and_falls_back_to_the_main_repository(self):
        channel = "win-x64-releases"
        legacy, current = asset("0.2.0", channel=channel), asset("0.2.1", channel=channel)
        hosts = {updates.UPDATES: [{"tag_name": updates.UPDATES_TAG, "prerelease": False, "assets": [
                     {"name": f"releases.{channel}.json"}, {"name": current["FileName"]}]},
                    {"tag_name": "ci-dependencies-abc", "prerelease": True, "assets": []}],
                 "jsjoberg/perikop": [{"tag_name": "v0.2.0", "prerelease": False, "assets": [
                     {"name": f"releases.{channel}.json"}, {"name": legacy["FileName"]}]}]}
        feeds = {updates.UPDATES: current, "jsjoberg/perikop": legacy}
        downloaded = []
        def download(repository, tag, name, directory):
            downloaded.append((repository, name))
            if name.endswith(".json"):
                (directory / name).write_text(json.dumps({"Assets": [feeds[repository]]}))
            else:
                write_asset(directory, feeds[repository])
            return directory / name
        for version, expected in (("0.2.2", current), ("0.2.1", legacy)):
            with self.subTest(version=version), tempfile.TemporaryDirectory() as temporary, \
                    patch.dict(os.environ, NIGHTLY="false"), \
                    patch.object(updates, "releases", side_effect=lambda repository: hosts[repository]), \
                    patch.object(updates, "download", side_effect=download):
                updates.restore("jsjoberg/perikop", "Windows", Path(temporary), version)
                updates.verify(Path(temporary) / channel / expected["FileName"], expected)

    def test_versions_order_nightly_numbers_numerically_and_stable_above_prerelease(self):
        versions = ["1.0.0-alpha.1", "1.0.0-nightly.20261018.9.1", "1.0.0-nightly.20261018.10.1", "1.0.0", "1.0.1"]
        self.assertEqual(sorted(reversed(versions), key=updates.precedence), versions)
        for invalid in ("1.0", "1.0.0-01", "1.0.0-a..b", "01.0.0"):
            with self.assertRaises(ValueError):
                updates.precedence(invalid)


class WindowsProbeTest(unittest.TestCase):
    def test_metadata_can_appear_before_the_new_executable_is_ready(self):
        probe = Mock(side_effect=[(1, ""), (1, ""), (0, "VERSION 1.0.0-test.2\nNO_UPDATE\n")])
        metadata = Mock(return_value="1.0.0-test.2")
        with patch.object(windows.time, "sleep"):
            windows.wait_for_probe(probe, "1.0.0-test.2", metadata)
        self.assertEqual(probe.call_count, 3)

    def test_old_copy_is_not_launched_during_replacement(self):
        probe = Mock(return_value=(0, "VERSION 1.0.0-test.2\nNO_UPDATE\n"))
        metadata = Mock(side_effect=["1.0.0-test.1", "", "1.0.0-test.2"])
        with patch.object(windows.time, "sleep"):
            windows.wait_for_probe(probe, "1.0.0-test.2", metadata)
        probe.assert_called_once()

    def test_readiness_timeout_reports_the_last_failure(self):
        probe = Mock(return_value=(1, "SDK still locked"))
        with patch.object(windows.time, "monotonic", side_effect=[0, 121]):
            with self.assertRaisesRegex(SystemExit, "SDK still locked"):
                windows.wait_for_probe(probe, "1.0.0-test.2", lambda: "1.0.0-test.2")


class FakeUpdates:
    """The updates release as asset names and bytes, behind gh, releases and download."""

    def __init__(self, assets=None):
        self.assets = dict(assets or {})
        self.exists = assets is not None

    def releases(self, repository):
        if repository != updates.UPDATES or not self.exists:
            return []
        return [{"tag_name": updates.UPDATES_TAG, "draft": False, "prerelease": False,
                 "assets": [{"name": name} for name in self.assets]}]

    def download(self, repository, tag, name, directory):
        path = directory / name
        path.write_bytes(self.assets[name])
        return path

    def gh(self, *arguments):
        arguments = list(map(str, arguments))
        if arguments[:2] == ["release", "create"]:
            self.exists = True
        elif arguments[:2] == ["release", "upload"]:
            path = Path(arguments[3])
            if path.name in self.assets and "--clobber" not in arguments:
                raise RuntimeError("asset exists")
            self.assets[path.name] = path.read_bytes()
        elif arguments[:2] == ["release", "delete-asset"]:
            del self.assets[arguments[3]]
        else:
            raise AssertionError(arguments)
        return ""

    def patch(self):
        return patch.multiple(updates, releases=self.releases, download=self.download, gh=self.gh)

    def feed(self, channel=CHANNEL):
        return json.loads(self.assets[f"releases.{channel}.json"])["Assets"]


def stage_directory(directory, version, previous=None, channel=CHANNEL):
    entries = [asset(version, channel=channel)]
    if previous:
        entries = [asset(previous, channel=channel)] + entries + [asset(version, "Delta", channel=channel)]
    for entry in entries:
        if entry["Version"] == version:
            write_asset(directory, entry)
    (directory / f"releases.{channel}.json").write_text(json.dumps({"Assets": entries}))
    return entries


def asset_bytes(*entries):
    return {entry["FileName"]: (entry["Version"] + entry["Type"]).encode() for entry in entries}


class UpdatesReleaseTest(unittest.TestCase):
    def test_merge_keeps_the_newest_full_and_recent_deltas(self):
        assets = [asset(f"1.0.{number}") for number in range(1, 10)]
        assets += [asset(f"1.0.{number}", "Delta") for number in range(2, 10)]
        merged = updates.merge(assets[:-1], [assets[8], assets[-1]])
        self.assertEqual(merged[0], asset("1.0.9"))
        self.assertEqual([entry["Version"] for entry in merged[1:]], [f"1.0.{n}" for n in range(9, 9 - updates.KEEP, -1)])
        self.assertTrue(all(entry["Type"] == "Delta" for entry in merged[1:]))

    def test_staged_packages_go_live_only_on_publish(self):
        old = asset("1.0.0-test.1")
        fake = FakeUpdates({**asset_bytes(old), f"releases.{CHANNEL}.json": json.dumps({"Assets": [old]}).encode()})
        with tempfile.TemporaryDirectory() as temporary, fake.patch():
            entries = stage_directory(Path(temporary), "1.0.0-test.2", "1.0.0-test.1")
            updates.stage(Path(temporary), "nightly-tag", "1.0.0-test.2")
            self.assertEqual(fake.feed(), [old])
            self.assertIn(entries[1]["FileName"], fake.assets)
            updates.publish("nightly-tag")
        self.assertEqual(fake.feed(), [entries[1], entries[2]])
        self.assertNotIn(f"nightly-tag.releases.{CHANNEL}.json", fake.assets)
        # The old full package is no longer named by any feed.
        self.assertNotIn(old["FileName"], fake.assets)

    def test_first_stage_creates_the_updates_release(self):
        fake = FakeUpdates()
        with tempfile.TemporaryDirectory() as temporary, fake.patch():
            entries = stage_directory(Path(temporary), "1.0.0-test.1")
            updates.stage(Path(temporary), "nightly-tag", "1.0.0-test.1")
            updates.publish("nightly-tag")
        self.assertEqual(fake.feed(), entries)

    def test_a_staged_release_keeps_its_packages_while_nightlies_publish(self):
        fake = FakeUpdates({})
        channel = "osx-arm64-releases"
        with tempfile.TemporaryDirectory() as temporary, fake.patch():
            release, nightly = Path(temporary) / "release", Path(temporary) / "nightly"
            release.mkdir()
            nightly.mkdir()
            staged = stage_directory(release, "1.0.0", channel=channel)
            stage_directory(nightly, "1.0.1-nightly.1")
            updates.stage(release, "v1.0.0", "1.0.0")
            updates.stage(nightly, "nightly-tag", "1.0.1-nightly.1")
            updates.publish("nightly-tag")
            self.assertIn(staged[0]["FileName"], fake.assets)
            self.assertNotIn(f"releases.{channel}.json", fake.assets)
            updates.publish("v1.0.0")
        self.assertEqual(fake.feed(channel), staged)

    def test_uploaded_bytes_are_never_replaced(self):
        entry = asset("1.0.0-test.1")
        fake = FakeUpdates({entry["FileName"]: b"other bytes"})
        with tempfile.TemporaryDirectory() as temporary, fake.patch():
            stage_directory(Path(temporary), "1.0.0-test.1")
            with self.assertRaisesRegex(ValueError, "size"):
                updates.stage(Path(temporary), "nightly-tag", "1.0.0-test.1")
        self.assertEqual(fake.assets[entry["FileName"]], b"other bytes")

    def test_a_retried_stage_accepts_the_same_bytes(self):
        fake = FakeUpdates({})
        with tempfile.TemporaryDirectory() as temporary, fake.patch():
            stage_directory(Path(temporary), "1.0.0-test.1")
            updates.stage(Path(temporary), "nightly-tag", "1.0.0-test.1")
            updates.stage(Path(temporary), "nightly-tag", "1.0.0-test.1")

    def test_cleanup_deletes_only_old_nightlies(self):
        listed = [{"tag_name": f"nightly-2026-10-{day:02d}-aaaaaaa", "prerelease": True,
                   "published_at": f"2026-10-{day:02d}T00:00:00Z"} for day in range(1, 11)]
        listed += [{"tag_name": "v0.2.0", "prerelease": False, "published_at": "2026-10-05T00:00:00Z"},
                   {"tag_name": "v0.2.0-alpha.1", "prerelease": True, "published_at": "2026-10-01T00:00:00Z"}]
        deleted = []
        with patch.object(updates, "releases", return_value=listed), \
                patch.object(updates, "gh", side_effect=lambda *arguments: deleted.append(arguments[2])):
            updates.cleanup("owner/repo", 7)
        self.assertEqual(deleted, [f"nightly-2026-10-{day:02d}-aaaaaaa" for day in (3, 2, 1)])

if __name__ == "__main__":
    unittest.main()
