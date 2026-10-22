"""Exercise the AppImage license collector without RPM or a Linux host, and check the AppImage runtime's
LGPL-2.1 obligations: licence texts, notices, pins, and the corresponding-source archive."""

import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("appimage_runtime", ROOT / "tools/appimage_runtime.py")
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)


class AppImageLicensesTest(unittest.TestCase):
    def collect(self, root, origin, installed_license=None):
        tools = root / "tools"
        tools.mkdir()
        # The collector needs only library names and package metadata from these tools.
        (tools / "find").write_text('''#!/bin/sh
case "$1" in
    /usr/lib64) printf '/usr/lib64/libfixture.so.1\n' ;;
    *) printf 'libfixture.so.1\n' ;;
esac
''')
        (tools / "rpm").write_text('''#!/bin/sh
set -eu
case "$1" in
    -qf) printf '%s-libs\n' "$FIXTURE_ORIGIN" ;;
    -qa) printf '%s-libs %s-1.9.3-5.el9.src.rpm\n' "$FIXTURE_ORIGIN" "$FIXTURE_ORIGIN" ;;
    -q)
        case "$2" in
            --licensefiles) printf '%s\n' "$FIXTURE_LICENSE" ;;
            --docfiles) printf '/not-installed/share/doc/LICENSE\n' ;;
            --qf)
                case "$3" in
                    '%{SOURCERPM}') printf '%s-1.9.3-5.el9.src.rpm\n' "$FIXTURE_ORIGIN" ;;
                    '%{LICENSE}') printf 'BSD\n' ;;
                    *) printf '%s-libs 1.9.3-5.el9: BSD\n' "$FIXTURE_ORIGIN" ;;
                esac ;;
            *) exit 1 ;;
        esac ;;
    *) exit 1 ;;
esac
''')
        for tool in tools.iterdir():
            tool.chmod(0o755)
        build = root / "build"
        build.mkdir()
        # Run the production collector, not a copy of its license-selection logic.
        script = (ROOT / "tools/prepare_appdir.sh").read_text()
        collector = script[script.index("licenses=$appdir/"):script.index("# Also check GTK libraries")]
        env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ["PATH"],
                   appdir=str(build / "AppDir"), build=str(build), source=str(ROOT),
                   FIXTURE_ORIGIN=origin, FIXTURE_LICENSE=str(installed_license or root / "absent-LICENSE"))
        result = subprocess.run(["sh", "-eu", "-c", collector], env=env, capture_output=True, text=True)
        return result, build / "AppDir/usr/bin/resources/licenses/linux"

    def test_lz4_fallback_copies_the_complete_license_when_rpm_files_are_absent(self):
        with tempfile.TemporaryDirectory() as temporary:
            result, licenses = self.collect(Path(temporary), "lz4")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual((licenses / "lz4/LICENSE").read_bytes(),
                             (ROOT / "tools/appimage-licenses/lz4/LICENSE").read_bytes())
            self.assertIn("lz4-libs 1.9.3-5.el9: BSD", (licenses / "PACKAGES.txt").read_text())

    def test_installed_rpm_license_still_copies_without_a_fallback(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            license = root / "LICENSE"
            license.write_text("Installed package license\n")
            result, licenses = self.collect(root, "fixture", license)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual((licenses / "fixture/LICENSE").read_bytes(), license.read_bytes())

    def test_unknown_missing_license_still_stops_packaging(self):
        with tempfile.TemporaryDirectory() as temporary:
            result, licenses = self.collect(Path(temporary), "missing-package")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("No licence text for bundled packages: missing-package-libs", result.stderr)
            self.assertFalse((licenses / "missing-package").exists())

class AppImageRuntimeLgplTest(unittest.TestCase):
    """The runtime statically links libfuse (LGPL-2.1); each check fails when an obligation's material goes."""

    def test_lgpl_text_is_complete_and_pinned(self):
        text = (ROOT / "resources/licenses/libfuse-LGPL-2.1.txt").read_text()
        self.assertTrue(text.lstrip().startswith("GNU LESSER GENERAL PUBLIC LICENSE"))
        for passage in ("Version 2.1, February 1999", "Copyright (C) 1991, 1999 Free Software Foundation",
                        "6. As an exception to the Sections above", "END OF TERMS AND CONDITIONS",
                        "How to Apply These Terms to Your New Libraries", "That's all there is to it!"):
            self.assertIn(passage, text)
        self.assertEqual(text.count("\n"), 502)
        manifest = {asset.get("path"): asset for asset in json.loads((ROOT / "resources/manifest.json").read_text())["assets"]}
        for licence in runtime.LICENCES:
            asset = manifest.get("resources/licenses/" + licence)
            self.assertIsNotNone(asset, licence + " is not in resources/manifest.json")
            self.assertEqual(asset["sha256"], hashlib.sha256((ROOT / asset["path"]).read_bytes()).hexdigest(), licence)
        self.assertIn("fuse-3.15.0/LGPL2.txt", manifest["resources/licenses/libfuse-LGPL-2.1.txt"]["url"])

    def test_notices_name_libfuse_the_runtime_and_the_source(self):
        notices = (ROOT / "THIRD-PARTY-NOTICES.md").read_text()
        for passage in ("libfuse 3.15.0", "LGPL-2.1", "patches/libfuse/mount.c.diff", "libfuse-LGPL-2.1.txt",
                        runtime.RUNTIME["commit"], runtime.name() + ".tar.gz", "Perikop remains under the MIT license"):
            self.assertIn(passage, notices)
        self.assertTrue((ROOT / "LICENSE").read_text().startswith("MIT License"))

    def test_runtime_and_sources_are_pinned_for_both_architectures(self):
        upstreams = json.loads((ROOT / "ci/upstreams.json").read_text())
        self.assertEqual(set(runtime.RUNTIME) - {"commit"}, {"x86_64", "aarch64"})
        # The runtime comes from vpk, so vpk stays pinned too.
        self.assertEqual(upstreams["vpk"]["sha256"], "2b56ce117f803fc70c103cb423bd040e395e40370f6ff6e10818e9ff9c26a323")
        sources = {item["file"]: item for item in runtime.inputs()}
        commit = runtime.RUNTIME["commit"]
        archives = [item for item in sources.values() if item["file"].startswith("type2-runtime-")]
        self.assertEqual([item["file"] for item in archives], ["type2-runtime-" + commit[:12] + ".tar.gz"])
        self.assertTrue(archives[0]["url"].endswith(commit))
        # The sources pinned only for linuxdeploy's former runtime stay out of the archive.
        self.assertFalse(set(runtime.RETIRED_SOURCES) & set(sources))
        # The same libfuse and squashfuse hashes as upstream's scripts/common/install-dependencies.sh.
        self.assertEqual(sources["fuse-3.15.0.tar.xz"]["sha256"],
                         "70589cfd5e1cff7ccd6ac91c86c01be340b227285c5e200baa284e401eea2ca0")
        self.assertEqual(sources["squashfuse-0.5.2.tar.gz"]["sha256"],
                         "db0238c5981dabbd80ee09ae15387f390091668ca060a7bc38047912491443d3")
        for library in ("musl-1.2.5", "zlib-1.3.1", "zstd-1.5.6", "mimalloc-2.1.7"):
            self.assertTrue(any(name.startswith(library) for name in sources), library)
        for item in sources.values():
            self.assertRegex(item["sha256"], "^[0-9a-f]{64}$")
            # Both Linux jobs make the archive, so a nightly with either AppImage still carries it.
            self.assertEqual(item.get("platform"), ["Linux", "Linux-arm64"])

    def fixture_inputs(self, root):
        """Small stand-ins for the pinned source archives, with their own upstreams.json."""
        archives = root / "archives"
        archives.mkdir()
        items = {}
        for item in runtime.inputs():
            data = ("fixture " + item["file"]).encode()
            (archives / item["file"]).write_bytes(data)
            items[item["file"]] = dict(item, sha256=hashlib.sha256(data).hexdigest())
        upstreams = root / "upstreams.json"
        upstreams.write_text(json.dumps(items))
        return archives, upstreams

    def test_source_archive_is_complete_and_reproducible(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archives, upstreams = self.fixture_inputs(root)
            with patch.object(runtime, "UPSTREAMS", upstreams):
                first = runtime.source(archives, root / "first")
                second = runtime.source(archives, root / "second")
            self.assertEqual(first.read_bytes(), second.read_bytes())
            self.assertEqual(first.name, "AppImage-runtime-source-3d17002.tar.gz")
            prefix = runtime.name() + "/"
            with tarfile.open(first) as archive:
                names = set(archive.getnames())
                readme = archive.extractfile(prefix + "README.md").read().decode()
                sums = archive.extractfile(prefix + "SHA256SUMS").read().decode()
            expected = {prefix + "README.md", prefix + "SHA256SUMS"}
            expected |= {prefix + "sources/" + item["file"] for item in runtime.inputs()}
            expected |= {prefix + "licenses/" + licence for licence in runtime.LICENCES}
            self.assertEqual(names, expected)
            for passage in (runtime.RUNTIME["commit"], runtime.RUNTIME["x86_64"], runtime.RUNTIME["aarch64"],
                            "libfuse-LGPL-2.1.txt", "--appimage-offset", "Relink with a modified libfuse",
                            "mount.c.diff", "Perikop itself is MIT-licensed"):
                self.assertIn(passage, readme)
            self.assertIn("sources/fuse-3.15.0.tar.xz", sums)

    def test_missing_or_changed_source_stops_the_archive(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archives, upstreams = self.fixture_inputs(root)
            (archives / "fuse-3.15.0.tar.xz").write_bytes(b"changed")
            with patch.object(runtime, "UPSTREAMS", upstreams), self.assertRaisesRegex(SystemExit, "fuse-3.15.0"):
                runtime.source(archives, root / "output")

    def test_runtime_check_rejects_other_builds_and_commits(self):
        commit = runtime.RUNTIME["commit"][:7]
        named = b"\x7fELF..." + f"https://github.com/AppImage/type2-runtime/commit/{commit}".encode()
        self.assertIsNone(runtime.appimage_problem(named))
        former = b"\x7fELF..." + b"https://github.com/AppImage/type2-runtime/commit/8f39b89"
        self.assertIn("does not name commit", runtime.appimage_problem(former))
        for architecture in ("x86_64", "aarch64"):
            self.assertIn("not the pinned build", runtime.runtime_problem(named, architecture))

    def test_appimage_without_the_lgpl_text_fails(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            # A stand-in AppImage whose --appimage-extract writes every licence except the LGPL text.
            image = root / "Perikop-fixture-x86_64.AppImage"
            licences = "squashfs-root/usr/share/perikop/resources/licenses"
            copies = "\n".join(f'cp "{ROOT}/resources/licenses/{licence}" {licences}/'
                                for licence in runtime.LICENCES if licence != "libfuse-LGPL-2.1.txt")
            image.write_text(f"#!/bin/sh\nmkdir -p {licences}\n{copies}\n"
                             "touch squashfs-root/usr/share/perikop/resources/THIRD-PARTY-NOTICES.md\n")
            image.chmod(0o755)
            (root / "work").mkdir()
            problem = runtime.licences_problem(image, root / "work")
            self.assertEqual(problem, "Perikop-fixture-x86_64.AppImage lacks libfuse-LGPL-2.1.txt")

    def test_licences_reach_the_appdir(self):
        # Packaging copies resources/ whole and installs it beside the program; licences must not be removed.
        cmake = (ROOT / "CMakeLists.txt").read_text()
        self.assertIn('copy_directory "${CMAKE_SOURCE_DIR}/resources" "${resource_destination}"', cmake)
        self.assertIn('install(DIRECTORY "${resource_destination}/" DESTINATION share/perikop/resources)', cmake)
        self.assertNotIn('remove_directory "${resource_destination}/licenses"', cmake)


if __name__ == "__main__":
    unittest.main()
