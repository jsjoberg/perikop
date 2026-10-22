"""Ensure cached CI inputs work offline and damaged inputs never reach upstreams."""

import importlib.util
import json
import os
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

    def fixture(self, root, platform="Linux", vpk=False):
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
        # The update SDK archive has several top-level folders instead of one root.
        with zipfile.ZipFile(root / "velopack.zip", "w") as archive:
            archive.writestr("include/Velopack.hpp", "offline sdk")
            archive.writestr("lib-static/velopack_libc_linux_x64_gnu.a", "offline library")
        sdk = {"file": "velopack.zip", "sha256": dependencies.digest(root / "velopack.zip"),
               "source": "ortho_velopack", "url": "https://upstream.invalid/velopack.zip", "platform": ["Linux"]}
        (root / "cmake/updates.cmake").write_text(sdk["sha256"])
        items = {"sqlite": item, "velopack": sdk}
        if vpk:
            # Velopack's packaging tool: a .NET program with native Linux helpers.
            with zipfile.ZipFile(root / "vpk.nupkg", "w") as archive:
                archive.writestr("tools/net8.0/any/vpk.dll", "packaging tool")
                archive.writestr("vendor/UpdateNix_x64", b"\x7fELF helper")
                archive.writestr("vendor/UpdateMac", b"\xca\xfe\xba\xbe helper")
                archive.writestr("vendor/DefaultApp.png", "icon")
            items["vpk"] = {"file": "vpk.nupkg", "sha256": dependencies.digest(root / "vpk.nupkg"),
                            "url": "https://upstream.invalid/vpk.nupkg", "platform": [platform]}
        manifest = root / "ci/upstreams.json"
        manifest.write_text(json.dumps(items))
        downloads = root / "build/ci/downloads"
        (downloads / (platform + "-environment.tar.gz")).write_bytes(b"pinned container")
        if platform == "macOS":
            with tarfile.open(downloads / "macOS-tools.tar.gz", "w:gz") as archive:
                archive.add(root / "dependency.zip", arcname="fixture.zip")
        with tarfile.open(downloads / ("inputs-" + platform + ".tar.gz"), "w:gz") as archive:
            archive.add(manifest, arcname="upstreams.json")
            archive.add(root / "dependency.zip", arcname="dependency.zip")
            archive.add(root / "velopack.zip", arcname="velopack.zip")
            if vpk:
                archive.add(root / "vpk.nupkg", arcname="vpk.nupkg")
            for name in dependencies.APPIMAGE_TOOLS.get(platform, ()):
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
            sdk = root / "build/ci/sources/ortho_velopack"
            self.assertEqual((sdk / "include/Velopack.hpp").read_text(), "offline sdk")
            self.assertTrue((sdk / "lib-static/velopack_libc_linux_x64_gnu.a").is_file())

    def test_arm64_restores_its_own_environment_and_tools(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest, lock, downloads = self.fixture(root, "Linux-arm64")
            with patch.object(dependencies, "ROOT", root), patch.object(dependencies, "UPSTREAMS", manifest), \
                    patch.object(dependencies, "LOCK", lock), patch.object(dependencies.subprocess, "run") as run:
                dependencies.restore("Linux-arm64", root / "build/ci")
                run.assert_not_called()
            tools = sorted(path.name for path in (root / "build/ci/tools/appimage").iterdir())
            self.assertEqual(tools, sorted(dependencies.APPIMAGE_TOOLS["Linux-arm64"]))

    def test_linux_restores_vpk_with_executable_helpers(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest, lock, downloads = self.fixture(root, vpk=True)
            with patch.object(dependencies, "ROOT", root), patch.object(dependencies, "UPSTREAMS", manifest), \
                    patch.object(dependencies, "LOCK", lock), patch.object(dependencies.subprocess, "run") as run:
                dependencies.restore("Linux", root / "build/ci")
                run.assert_not_called()
            vpk = root / "build/ci/tools/vpk"
            self.assertTrue((vpk / "tools/net8.0/any/vpk.dll").is_file())
            self.assertTrue(os.access(vpk / "vendor/UpdateNix_x64", os.X_OK))
            self.assertFalse(os.access(vpk / "vendor/DefaultApp.png", os.X_OK))

    def test_macos_restores_vpk_with_executable_updater(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest, lock, downloads = self.fixture(root, "macOS", vpk=True)
            with patch.object(dependencies, "ROOT", root), patch.object(dependencies, "UPSTREAMS", manifest), \
                    patch.object(dependencies, "LOCK", lock), patch.object(dependencies.subprocess, "run") as run:
                dependencies.restore("macOS", root / "build/ci")
                run.assert_not_called()
            vpk = root / "build/ci/tools/vpk"
            self.assertTrue((vpk / "tools/net8.0/any/vpk.dll").is_file())
            self.assertTrue(os.access(vpk / "vendor/UpdateMac", os.X_OK))
            self.assertFalse(os.access(vpk / "vendor/DefaultApp.png", os.X_OK))

    def test_platform_lists_serve_each_named_platform(self):
        with tempfile.TemporaryDirectory() as temporary:
            manifest = Path(temporary) / "upstreams.json"
            manifest.write_text(json.dumps({
                "shared": {"file": "shared", "platform": ["Linux", "Linux-arm64"]},
                "intel": {"file": "intel", "platform": "Linux"},
                "everywhere": {"file": "everywhere"}}))
            with patch.object(dependencies, "UPSTREAMS", manifest):
                files = {platform: sorted(item["file"] for item in dependencies.selected(platform))
                         for platform in ("Linux", "Linux-arm64", "macOS")}
            self.assertEqual(files, {"Linux": ["everywhere", "intel", "shared"],
                                     "Linux-arm64": ["everywhere", "shared"], "macOS": ["everywhere"]})

    def test_cross_build_restores_windows_sources_without_windows_tools(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest, lock, downloads = self.fixture(root, "Windows")
            environment = downloads / dependencies.CROSS_ENVIRONMENT
            environment.write_bytes(b"pinned cross-build container")
            data = json.loads(lock.read_text())
            data["assets"][environment.name] = dependencies.digest(environment)
            lock.write_text(json.dumps(data))
            with patch.object(dependencies, "ROOT", root), patch.object(dependencies, "UPSTREAMS", manifest), \
                    patch.object(dependencies, "LOCK", lock), patch.object(dependencies.subprocess, "run") as run:
                dependencies.restore("Windows", root / "build/ci", cross=True)
                run.assert_not_called()
            self.assertEqual((root / "build/ci/sources/sqlite/sqlite3.h").read_text(), "offline source")
            self.assertFalse((root / "build/ci/tools").exists())

    def test_missing_cross_build_environment_stops_before_download(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest, lock, downloads = self.fixture(root, "Windows")
            with patch.object(dependencies, "ROOT", root), patch.object(dependencies, "UPSTREAMS", manifest), \
                    patch.object(dependencies, "LOCK", lock), patch.object(dependencies.subprocess, "run") as run:
                with self.assertRaisesRegex(ValueError, "snapshot lacks Windows-cross-environment"):
                    dependencies.restore("Windows", root / "build/ci", cross=True)
                run.assert_not_called()
            self.assertFalse((root / "build/ci/sources").exists())

    def test_cross_build_rejects_linux_sources(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, "require --platform Windows"):
                dependencies.restore("Linux", Path(temporary), cross=True)

    def test_windows_tools_only_restore_does_not_unpack_sources_or_compilers(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest, lock, downloads = self.fixture(root, "Windows")
            data = json.loads(manifest.read_text())
            # Unused compiler inputs need not exist for a tools-only restore.
            data["w64devkit"] = {"file": "w64devkit.7z.exe", "sha256": "unused", "platform": "Windows"}
            manifest.write_text(json.dumps(data))
            with tarfile.open(downloads / "inputs-Windows.tar.gz", "w:gz") as archive:
                archive.add(manifest, arcname="upstreams.json")
                archive.add(root / "dependency.zip", arcname="dependency.zip")
            data = json.loads(lock.read_text())
            data["upstreams_sha256"] = dependencies.digest(manifest)
            data["assets"]["inputs-Windows.tar.gz"] = dependencies.digest(downloads / "inputs-Windows.tar.gz")
            lock.write_text(json.dumps(data))
            with patch.object(dependencies, "ROOT", root), patch.object(dependencies, "UPSTREAMS", manifest), \
                    patch.object(dependencies, "LOCK", lock), patch.object(dependencies.subprocess, "run") as run:
                dependencies.restore("Windows", root / "build/ci", tools_only=True)
                run.assert_not_called()
            self.assertFalse((root / "build/ci/sources").exists())

    def test_tools_only_restore_rejects_incompatible_modes(self):
        for platform, options in (("Linux", {}), ("Windows", {"cross": True})):
            with self.subTest(platform=platform, options=options):
                with self.assertRaisesRegex(ValueError, "Tools-only restore requires Windows"):
                    dependencies.restore(platform, Path("unused"), tools_only=True, **options)

    def test_snapshot_lock_includes_cross_environment_when_prepared(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            names = ["inputs-" + platform + ".tar.gz" for platform in dependencies.PLATFORMS]
            names += ["Linux-environment.tar.gz", "Linux-arm64-environment.tar.gz", "macOS-tools.tar.gz"]
            for name in names:
                (output / name).write_bytes(b"fixture snapshot")
            dependencies.make_lock(output, "owner/project", "fixture")
            self.assertNotIn(dependencies.CROSS_ENVIRONMENT,
                             json.loads((output / "dependencies-lock.json").read_text())["assets"])
            (output / dependencies.CROSS_ENVIRONMENT).write_bytes(b"cross-build container")
            dependencies.make_lock(output, "owner/project", "fixture")
            assets = json.loads((output / "dependencies-lock.json").read_text())["assets"]
            self.assertEqual(assets[dependencies.CROSS_ENVIRONMENT],
                             dependencies.digest(output / dependencies.CROSS_ENVIRONMENT))

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
            for name in ("sqlite/sqlite3.h", "wxwidgets/CMakeLists.txt", "ortho_audio/miniaudio.h",
                         "ortho_velopack/include/Velopack.hpp"):
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
