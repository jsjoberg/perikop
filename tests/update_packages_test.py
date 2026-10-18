"""Update publications remain verifiable and nightly cleanup never strands retained feeds."""

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

    def test_assemble_checksums_cover_native_packages_feeds_and_update_packages(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            legacy, publication = root / "legacy", root / "updates"
            legacy.mkdir()
            (legacy / "Perikop-nightly-2026-10-18-aaaaaaa-macOS-arm64.dmg").write_bytes(b"native")
            entries = self.fixture(root, with_base=False)
            updates.finish(root, CHANNEL, "1.0.0-test.2", publication)
            output = root / "release"
            updates.assemble(legacy, publication, output, "1.0.0-test.2")
            lines = (output / "SHA256SUMS.txt").read_text().splitlines()
            self.assertEqual(len(lines), 3)
            for line in lines:
                digest, name = line.split("  ")
                self.assertEqual(digest, hashlib.sha256((output / name).read_bytes()).hexdigest())

    def test_assemble_ships_the_appimage_runtime_source_beside_appimages(self):
        channel = "linux-x64-nightly"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            legacy, publication = root / "legacy", root / "updates"
            legacy.mkdir()
            publication.mkdir()
            (legacy / "Perikop-0.1.0-x86_64.AppImage").write_bytes(b"appimage")
            full = asset("1.0.0-test.2", channel=channel)
            write_asset(publication, full)
            (publication / f"releases.{channel}.json").write_text(json.dumps({"Assets": [full]}))
            # The runtime statically links libfuse (LGPL-2.1), so AppImages never ship without its source.
            with self.assertRaisesRegex(ValueError, "corresponding source"):
                updates.assemble(legacy, publication, root / "refused", "1.0.0-test.2")
            (legacy / "AppImage-runtime-source-8f39b89-3d17002.tar.gz").write_bytes(b"source")
            updates.assemble(legacy, publication, root / "release", "1.0.0-test.2")
            checksums = (root / "release/SHA256SUMS.txt").read_text()
            self.assertIn(hashlib.sha256(b"source").hexdigest() + "  AppImage-runtime-source-8f39b89-3d17002.tar.gz\n",
                          checksums)

    def test_assemble_refuses_a_missing_platform_or_mismatched_version(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            legacy = root / "legacy"
            legacy.mkdir()
            (legacy / "Perikop-0.1.0-windows-x64.exe").write_bytes(b"wrong platform")
            self.fixture(root, with_base=False)
            updates.finish(root, CHANNEL, "1.0.0-test.2", root / "updates")
            for version in ("1.0.0-test.2", "1.0.0-test.3"):
                with self.subTest(version=version), self.assertRaises(ValueError):
                    updates.assemble(legacy, root / "updates", root / "release", version)
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


class RetentionTest(unittest.TestCase):
    def fixture(self):
        releases, feeds = [], {}
        for number in range(1, 10):
            tag = f"nightly-2026-10-{number:02d}-aaaaaaa"
            entries = [asset(f"1.0.0-nightly.{number}")]
            if number > 1:
                entries += [asset(f"1.0.0-nightly.{number - 1}"), asset(f"1.0.0-nightly.{number}", "Delta")]
            name = f"releases.{CHANNEL}.json"
            feeds[tag, name] = {"Assets": entries}
            releases.append({"tag_name": tag, "published_at": f"2026-10-{number:02d}T00:00:00Z",
                             "draft": False, "prerelease": True,
                             "assets": [{"name": name}, {"name": "SHA256SUMS.txt"}] + [{"name": entry["FileName"]} for entry in entries
                                                          if entry["Version"] == f"1.0.0-nightly.{number}"]})
        return releases, feeds, releases[-1]["tag_name"]

    def test_oldest_retained_delta_base_is_rescued_before_its_release_is_removed(self):
        releases, feeds, current = self.fixture()
        rescue, obsolete = updates.retention_plan(releases, feeds, current)
        self.assertEqual(obsolete, [releases[1]["tag_name"], releases[0]["tag_name"]])
        self.assertEqual(rescue, [(releases[1]["tag_name"], asset("1.0.0-nightly.2"))])
        self.assertNotIn(current, obsolete)

    def test_a_previously_rescued_base_needs_no_duplicate_upload(self):
        releases, feeds, current = self.fixture()
        releases[-1]["assets"].append({"name": asset("1.0.0-nightly.2")["FileName"]})
        rescue, obsolete = updates.retention_plan(releases, feeds, current)
        self.assertEqual(rescue, [])
        self.assertEqual(len(obsolete), 2)

    def test_missing_base_or_missing_current_stops_cleanup(self):
        releases, feeds, current = self.fixture()
        releases[1]["assets"] = []
        with self.assertRaisesRegex(ValueError, "missing package"):
            updates.retention_plan(releases, feeds, current)
        with self.assertRaisesRegex(ValueError, "Published nightly is missing"):
            updates.retention_plan(releases, feeds, "nightly-2026-10-30-bbbbbbb")

    def test_cleanup_never_deletes_stable_releases_drafts_or_dependency_snapshots(self):
        releases, feeds, current = self.fixture()
        extras = [{"tag_name": "v1.0.0", "draft": False, "prerelease": False},
                  {"tag_name": "ci-dependencies-aaaaaaaaaaaa", "draft": False, "prerelease": True},
                  {"tag_name": "nightly-2026-10-01-bbbbbbb", "draft": True, "prerelease": True}]
        rescue, obsolete = updates.retention_plan(releases + extras, feeds, current)
        self.assertEqual(set(obsolete), {releases[0]["tag_name"], releases[1]["tag_name"]})

    def test_conflicting_feed_hashes_and_excess_retention_stop_cleanup(self):
        releases, feeds, current = self.fixture()
        name = f"releases.{CHANNEL}.json"
        feeds[releases[-1]["tag_name"], name]["Assets"][1]["SHA256"] = "f" * 64
        with self.assertRaisesRegex(ValueError, "disagree"):
            updates.retention_plan(releases, feeds, current)
        with self.assertRaisesRegex(ValueError, "one and nine"):
            updates.retention_plan(releases, feeds, current, keep=10)

    def test_rescue_upload_failure_deletes_no_release(self):
        releases, feeds, current = self.fixture()
        calls = []
        def download(repository, tag, name, directory):
            path = directory / name
            if name == "SHA256SUMS.txt":
                entries = [entry for entry in feeds[current, f"releases.{CHANNEL}.json"]["Assets"]
                           if entry["Version"] == "1.0.0-nightly.9"]
                values = {entry["FileName"]: entry["SHA256"] for entry in entries}
                values[f"releases.{CHANNEL}.json"] = "f" * 64
                path.write_text(updates.checksum_text(values))
            elif name.endswith(".json"):
                path.write_text(json.dumps(feeds[tag, name]))
            else:
                write_asset(directory, asset("1.0.0-nightly.2"))
            return path
        def gh(*arguments):
            calls.append(arguments)
            if arguments[:2] == ("release", "upload"):
                raise RuntimeError("Upload failed")
            raise AssertionError(arguments)
        with patch.object(updates, "releases", return_value=releases), \
                patch.object(updates, "download", side_effect=download), patch.object(updates, "gh", side_effect=gh):
            with self.assertRaisesRegex(RuntimeError, "Upload failed"):
                updates.cleanup("owner/repo", current, 7)
        self.assertFalse(any(arguments[:2] == ("release", "delete") for arguments in calls))

    def test_rescued_package_checksum_is_uploaded_before_any_release_is_deleted(self):
        releases, feeds, current = self.fixture()
        calls, uploaded = [], {}
        def download(repository, tag, name, directory):
            path = directory / name
            if name == "SHA256SUMS.txt":
                entries = [entry for entry in feeds[current, f"releases.{CHANNEL}.json"]["Assets"]
                           if entry["Version"] == "1.0.0-nightly.9"]
                values = {entry["FileName"]: entry["SHA256"] for entry in entries}
                values[f"releases.{CHANNEL}.json"] = "f" * 64
                path.write_text(updates.checksum_text(values))
            elif name.endswith(".json"):
                path.write_text(json.dumps(feeds[tag, name]))
            else:
                write_asset(directory, asset("1.0.0-nightly.2"))
            return path
        def gh(*arguments):
            calls.append(arguments)
            if arguments[:2] == ("release", "upload"):
                uploaded[Path(arguments[3]).name] = Path(arguments[3]).read_bytes()
                return ""
            if arguments[:2] == ("release", "view"):
                names = {item["name"] for item in releases[-1]["assets"]} | set(uploaded)
                return json.dumps({"assets": [{"name": name} for name in names]})
            if arguments[:2] == ("release", "delete"):
                return ""
            raise AssertionError(arguments)
        with patch.object(updates, "releases", return_value=releases), \
                patch.object(updates, "download", side_effect=download), patch.object(updates, "gh", side_effect=gh):
            updates.cleanup("owner/repo", current, 7)
        checksums = updates.read_checksums(uploaded["SHA256SUMS.txt"].decode())
        rescued = asset("1.0.0-nightly.2")
        self.assertEqual(checksums[rescued["FileName"]], rescued["SHA256"])
        checksum_upload = next(index for index, call in enumerate(calls)
                               if call[:2] == ("release", "upload") and Path(call[3]).name == "SHA256SUMS.txt")
        first_deletion = next(index for index, call in enumerate(calls) if call[:2] == ("release", "delete"))
        self.assertLess(checksum_upload, first_deletion)
        calls.clear()
        uploaded.clear()
        def reject_checksum(*arguments):
            if arguments[:2] == ("release", "upload") and Path(arguments[3]).name == "SHA256SUMS.txt":
                raise RuntimeError("Checksum upload failed")
            return gh(*arguments)
        with patch.object(updates, "releases", return_value=releases), \
                patch.object(updates, "download", side_effect=download), patch.object(updates, "gh", side_effect=reject_checksum):
            with self.assertRaisesRegex(RuntimeError, "Checksum upload failed"):
                updates.cleanup("owner/repo", current, 7)
        self.assertFalse(any(call[:2] == ("release", "delete") for call in calls))

    def test_invalid_checksum_records_are_rejected(self):
        for text in ("", "invalid", "a" * 64 + "  ../escape\n", "a" * 64 + "  SHA256SUMS.txt\n"):
            with self.assertRaises(ValueError):
                updates.read_checksums(text)


if __name__ == "__main__":
    unittest.main()
