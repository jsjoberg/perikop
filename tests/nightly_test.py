"""Nightlies retain successful platform packages without hiding failed checks."""

import hashlib
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
    def fixtures(self, packages, suffixes):
        for suffix in suffixes:
            (packages / ("Perikop-0.1.0-" + suffix)).write_bytes(suffix.encode())

    def prepare(self, packages, sanitizers="success", tag=TAG):
        return nightly.prepare(packages, "owner/project", COMMIT, "1234", tag, sanitizers)

    def test_partial_nightly_keeps_successful_packages_and_lists_missing_platforms(self):
        with tempfile.TemporaryDirectory() as temporary:
            packages = Path(temporary)
            suffixes = ["macOS-arm64.dmg", "windows-x64.exe"]
            self.fixtures(packages, suffixes)
            self.assertEqual(self.prepare(packages), TAG)
            self.assertEqual(sorted(path.name for path in packages.glob("Perikop-*")),
                             sorted("Perikop-" + TAG + "-" + suffix for suffix in suffixes))
            for suffix in suffixes:
                self.assertEqual((packages / ("Perikop-" + TAG + "-" + suffix)).read_bytes(), suffix.encode())
            notes = (packages / "NOTES.md").read_text()
            self.assertIn("<!-- perikop-nightly: partial -->", notes)
            self.assertIn("Unavailable platforms:\n- Linux x64\n- Linux ARM64\n- macOS Intel", notes)
            self.assertIn("- macOS Apple Silicon", notes)
            self.assertIn("Sanitizers: **passed**", notes)
            for line in (packages / "SHA256SUMS.txt").read_text().splitlines():
                checksum, name = line.split("  ", 1)
                self.assertEqual(checksum, hashlib.sha256((packages / name).read_bytes()).hexdigest())

    def test_all_platforms_and_sanitizers_mark_a_complete_nightly(self):
        with tempfile.TemporaryDirectory() as temporary:
            packages = Path(temporary)
            self.fixtures(packages, nightly.PLATFORMS)
            self.prepare(packages)
            notes = (packages / "NOTES.md").read_text()
            self.assertIn("<!-- perikop-nightly: complete -->", notes)
            self.assertNotIn("Unavailable platforms", notes)
            self.assertNotIn("Incomplete nightly", notes)

    def test_sanitizer_failure_remains_visible_even_with_all_platforms(self):
        with tempfile.TemporaryDirectory() as temporary:
            packages = Path(temporary)
            self.fixtures(packages, nightly.PLATFORMS)
            self.prepare(packages, sanitizers="failure")
            notes = (packages / "NOTES.md").read_text()
            self.assertIn("<!-- perikop-nightly: partial -->", notes)
            self.assertIn("Sanitizers: **failure**", notes)
            self.assertEqual(len(list(packages.glob("Perikop-*"))), 5)

    def test_empty_nightly_does_not_create_a_release(self):
        with tempfile.TemporaryDirectory() as temporary:
            packages = Path(temporary)
            self.assertIsNone(self.prepare(packages))
            self.assertEqual(list(packages.iterdir()), [])

    def test_different_versions_fail_before_any_package_is_renamed(self):
        with tempfile.TemporaryDirectory() as temporary:
            packages = Path(temporary)
            self.fixtures(packages, ["windows-x64.exe"])
            (packages / "Perikop-0.2.0-macOS-x86_64.dmg").write_bytes(b"fixture")
            original = sorted(path.name for path in packages.iterdir())
            with self.assertRaisesRegex(ValueError, "different versions"):
                self.prepare(packages)
            self.assertEqual(sorted(path.name for path in packages.iterdir()), original)

    def test_unknown_platform_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            packages = Path(temporary)
            (packages / "Perikop-0.1.0-unknown.zip").write_bytes(b"fixture")
            with self.assertRaisesRegex(ValueError, "Unknown nightly package"):
                self.prepare(packages)

    def test_retry_tag_retains_the_commit_and_unique_run_attempt(self):
        with tempfile.TemporaryDirectory() as temporary:
            packages = Path(temporary)
            self.fixtures(packages, ["windows-x64.exe"])
            retry = TAG + "-1234-2"
            self.assertEqual(self.prepare(packages, tag=retry), retry)
            self.assertTrue((packages / ("Perikop-" + retry + "-windows-x64.exe")).is_file())


if __name__ == "__main__":
    unittest.main()
