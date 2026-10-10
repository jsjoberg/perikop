"""Ensure cached CI inputs work offline and damaged inputs never reach upstreams."""

import importlib.util
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("ci_dependencies", ROOT / "tools/ci/dependencies.py")
dependencies = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dependencies)


class DependenciesTest(unittest.TestCase):
    def test_native_versions_match_upstream_snapshot(self):
        dependencies.audit()

    def fixture(self, root):
        (root / "ci").mkdir()
        (root / "build/ci/downloads").mkdir(parents=True)
        (root / "CMakeLists.txt").write_text("")
        (root / "cmake").mkdir()
        (root / "cmake/speech.cmake").write_text("")
        with zipfile.ZipFile(root / "dependency.zip", "w") as archive:
            archive.writestr("sqlite/sqlite3.h", "offline source")
        item = {"file": "dependency.zip", "sha256": dependencies.digest(root / "dependency.zip"),
                "source": "sqlite", "url": "https://upstream.invalid/dependency.zip"}
        (root / "CMakeLists.txt").write_text(item["sha256"])
        manifest = root / "ci/upstreams.json"
        manifest.write_text(json.dumps({"sqlite": item}))
        downloads = root / "build/ci/downloads"
        (downloads / "Linux-environment.tar.gz").write_bytes(b"pinned container")
        with tarfile.open(downloads / "inputs-Linux.tar.gz", "w:gz") as archive:
            archive.add(manifest, arcname="upstreams.json")
            archive.add(root / "dependency.zip", arcname="dependency.zip")
            for name in ("linuxdeploy-x86_64.AppImage", "linuxdeploy-plugin-gtk.sh", "runtime-x86_64"):
                (root / name).write_bytes(b"fixture tool")
                archive.add(root / name, arcname=name)
        lock = root / "ci/dependencies-lock.json"
        lock.write_text(json.dumps({"repository": "owner/project", "release": "ci-dependencies-test",
            "upstreams_sha256": dependencies.digest(manifest),
            "assets": {p.name: dependencies.digest(p) for p in downloads.iterdir()}}))
        return manifest, lock, downloads

    def test_cached_snapshot_needs_no_network(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest, lock, downloads = self.fixture(root)
            with patch.object(dependencies, "ROOT", root), patch.object(dependencies, "UPSTREAMS", manifest), \
                    patch.object(dependencies, "LOCK", lock), patch.object(dependencies.subprocess, "run") as run:
                dependencies.restore("Linux", root / "build/ci")
                run.assert_not_called()
            self.assertEqual((root / "build/ci/sources/sqlite/sqlite3.h").read_text(), "offline source")

    def test_corrupt_cache_fetches_only_our_release_and_rejects_bad_download(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest, lock, downloads = self.fixture(root)
            (downloads / "inputs-Linux.tar.gz").write_bytes(b"corrupt cache")
            calls = []
            def download(command, **kwargs):
                calls.append(command)
                destination = Path(command[command.index("--dir") + 1])
                (destination / "inputs-Linux.tar.gz").write_bytes(b"damaged download")
            with patch.object(dependencies, "ROOT", root), patch.object(dependencies, "UPSTREAMS", manifest), \
                    patch.object(dependencies, "LOCK", lock), patch.object(dependencies.subprocess, "run", side_effect=download):
                with self.assertRaisesRegex(ValueError, "hash mismatch"):
                    dependencies.restore("Linux", root / "build/ci")
            self.assertEqual(len(calls), 1)
            self.assertEqual(calls[0][:4], ["gh", "release", "download", "ci-dependencies-test"])
            self.assertEqual(calls[0][calls[0].index("--repo") + 1], "owner/project")
            self.assertFalse((root / "build/ci/sources").exists())

    def test_changed_manifest_stops_before_any_download(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest, lock, downloads = self.fixture(root)
            manifest.write_text(manifest.read_text() + "\n")
            with patch.object(dependencies, "ROOT", root), patch.object(dependencies, "UPSTREAMS", manifest), \
                    patch.object(dependencies, "LOCK", lock), patch.object(dependencies.subprocess, "run") as run:
                with self.assertRaisesRegex(ValueError, "Prepare and pin"):
                    dependencies.restore("Linux", root / "build/ci")
                run.assert_not_called()

    def test_missing_offline_sources_fail_without_fetching(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = subprocess.run(["cmake", "-DPERIKOP_DEPENDENCY_DIR=" + temporary,
                "-P", str(ROOT / "cmake/offline-dependencies.cmake")], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("no upstream download is allowed", result.stderr)

    def test_rejects_archive_escape(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with zipfile.ZipFile(root / "bad.zip", "w") as archive:
                archive.writestr("../escape", "bad")
            with self.assertRaisesRegex(ValueError, "Unsafe archive"):
                dependencies.extract(root / "bad.zip", root / "output")
            self.assertFalse((root / "escape").exists())

    def test_new_unprepared_dependency_cannot_download(self):
        policy = subprocess.run(["cmake", "--help-policy", "CMP0170"], capture_output=True)
        if policy.returncode:
            self.skipTest("Older CMake prevents downloads but does not enforce missing source directories")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            sources = root / "sources"
            for name in ("sqlite/sqlite3.h", "wxwidgets/CMakeLists.txt", "ortho_sonic/sonic.h",
                         "ortho_audio/miniaudio.h", "ortho_ort/include/onnxruntime_c_api.h"):
                file = sources / name
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_text("")
            locale = sources / "wxwidgets/src/osx/core/uilocale.mm"
            locale.parent.mkdir(parents=True)
            locale.write_text("\n".join("    wxCFStringRef cf(" + name + ");\n    [df release];\n    return cf.AsString();"
                                        for name in ("monthName", "weekdayName")))
            (root / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.24)\nproject(offline NONE)\ninclude(FetchContent)\n'
                'set(PERIKOP_DEPENDENCY_DIR "' + sources.as_posix() + '")\n'
                'include("' + (ROOT / "cmake/offline-dependencies.cmake").as_posix() + '" NO_POLICY_SCOPE)\n'
                'FetchContent_Declare(unprepared URL https://upstream.invalid/archive.tar.gz)\n'
                'FetchContent_MakeAvailable(unprepared)\n')
            result = subprocess.run(["cmake", "-S", str(root), "-B", str(root / "build")],
                                    capture_output=True, text=True, timeout=15)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("FETCHCONTENT_FULLY_DISCONNECTED", result.stderr)
            self.assertNotIn("Downloading", result.stdout)


if __name__ == "__main__":
    unittest.main()
