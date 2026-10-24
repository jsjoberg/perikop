"""Nightlies publish their successful platforms without hiding failed checks."""

import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("nightly", ROOT / "tools/ci/nightly.py")
nightly = importlib.util.module_from_spec(spec)
spec.loader.exec_module(nightly)
COMMIT = "a" * 40
TAG = "nightly-2026-10-16-aaaaaaa"


class NightlyTest(unittest.TestCase):
    def fixtures(self, updates, platforms):
        for platform in platforms:
            (updates / f"releases.{platform}-nightly.json").write_text("{}")

    def prepare(self, updates, macos_sanitizers="success", linux_sanitizers="success", tag=TAG, version=None):
        return nightly.prepare(updates, updates.parent / "notes.md", "owner/project", COMMIT, "1234", tag,
                               macos_sanitizers, linux_sanitizers, version)

    def notes(self, updates):
        return (updates.parent / "notes.md").read_text()

    def directory(self, temporary):
        updates = Path(temporary) / "updates"
        updates.mkdir()
        return updates

    def test_partial_nightly_lists_available_and_missing_platforms(self):
        with tempfile.TemporaryDirectory() as temporary:
            updates = self.directory(temporary)
            self.fixtures(updates, ["osx-arm64", "win-x64"])
            self.assertEqual(self.prepare(updates), TAG)
            notes = self.notes(updates)
            self.assertIn("<!-- perikop-nightly: partial -->", notes)
            self.assertIn("Available platforms:\n- macOS Apple Silicon\n- Windows x64", notes)
            self.assertIn("Unavailable platforms:\n- Linux x64\n- Linux ARM64", notes)
            self.assertIn("Sanitizers:\n- macOS: **passed**\n- Linux: **passed**", notes)
            # The notes stay outside the publication, which ships every file in it.
            self.assertEqual(sorted(path.name for path in updates.iterdir()),
                             ["releases.osx-arm64-nightly.json", "releases.win-x64-nightly.json"])

    def test_all_platforms_and_sanitizers_mark_a_complete_nightly(self):
        with tempfile.TemporaryDirectory() as temporary:
            updates = self.directory(temporary)
            self.fixtures(updates, nightly.PLATFORMS)
            self.prepare(updates)
            notes = self.notes(updates)
            self.assertIn("<!-- perikop-nightly: complete -->", notes)
            self.assertNotIn("Unavailable platforms", notes)
            self.assertNotIn("Incomplete nightly", notes)
            self.assertIn("download the new AppImage once on a supported system", notes)

    def test_sanitizer_failure_remains_visible_even_with_all_platforms(self):
        for platform in ("macos", "linux"):
            for result in ("failure", "cancelled", "skipped"):
                with self.subTest(platform=platform, result=result), tempfile.TemporaryDirectory() as temporary:
                    updates = self.directory(temporary)
                    self.fixtures(updates, nightly.PLATFORMS)
                    self.assertEqual(self.prepare(updates, **{platform + "_sanitizers": result}), TAG)
                    notes = self.notes(updates)
                    self.assertIn("<!-- perikop-nightly: partial -->", notes)
                    name = "macOS" if platform == "macos" else "Linux"
                    self.assertIn(f"- {name}: **{result}**", notes)

    def test_empty_nightly_does_not_create_a_release(self):
        with tempfile.TemporaryDirectory() as temporary:
            updates = self.directory(temporary)
            self.assertIsNone(self.prepare(updates))
            self.assertFalse((updates.parent / "notes.md").exists())

    def test_unknown_or_release_feeds_are_rejected(self):
        for name in ("releases.osx-x64-nightly.json", "releases.win-x64-releases.json",
                     "releases.linux-x64-nightly.json", "releases.linux-arm64-nightly.json"):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as temporary:
                updates = self.directory(temporary)
                (updates / name).write_text("{}")
                with self.assertRaisesRegex(ValueError, "Unknown nightly feed"):
                    self.prepare(updates)

    def test_retry_tag_retains_the_commit_and_unique_run_attempt(self):
        with tempfile.TemporaryDirectory() as temporary:
            updates = self.directory(temporary)
            self.fixtures(updates, ["win-x64"])
            retry = TAG + "-1234-2"
            self.assertEqual(self.prepare(updates, tag=retry), retry)
            with self.assertRaisesRegex(ValueError, "does not match the commit"):
                self.prepare(updates, tag="nightly-2026-10-16-bbbbbbb")

    def test_notes_name_the_build_version(self):
        with tempfile.TemporaryDirectory() as temporary:
            updates = self.directory(temporary)
            self.fixtures(updates, ["win-x64"])
            self.prepare(updates, version="0.1.1-nightly.20261016.7.1")
            self.assertIn("Version `0.1.1-nightly.20261016.7.1`.", self.notes(updates))


def precedence(version):
    """SemVer 2 ordering: a prerelease sorts below its release, numeric identifiers by value."""
    core, _, pre = version.partition("-")
    identifiers = [(0, int(part), "") if part.isdigit() else (1, 0, part) for part in pre.split(".")] if pre else []
    return tuple(int(part) for part in core.split(".")), not pre, identifiers


class NightlyVersionTest(unittest.TestCase):
    def test_nightlies_follow_the_last_release_and_keep_increasing(self):
        released = [nightly.version("0.1.0", True, day, run, attempt)
                    for day, run, attempt in [("2026-10-18", 412, 1), ("2026-10-18", 412, 2), ("2026-10-18", 413, 1),
                                              ("2026-10-19", 414, 1)]]
        self.assertEqual(released[0], "0.1.1-nightly.20261018.412.1")
        ordered = ["0.1.0"] + released + ["0.1.1"]
        self.assertEqual(sorted(ordered, key=precedence), ordered)

    def test_before_a_release_nightlies_are_its_prereleases(self):
        ordered = ["0.1.0", "0.2.0-alpha.1", nightly.version("0.2.0", False, "2026-11-01", 500, 1), "0.2.0"]
        self.assertEqual(ordered[2], "0.2.0-nightly.20261101.500.1")
        self.assertEqual(sorted(ordered, key=precedence), ordered)

    def test_invalid_inputs_are_rejected(self):
        for base, day in [("0.1", "2026-10-18"), ("0.1.0-alpha.1", "2026-10-18"), ("0.1.0", "20261018")]:
            with self.assertRaises(ValueError):
                nightly.version(base, False, day, 1, 1)


if __name__ == "__main__":
    unittest.main()
