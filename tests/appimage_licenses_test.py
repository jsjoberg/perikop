"""Exercise the AppImage license collector without RPM or a Linux host."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


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
        script = (ROOT / "tools/package_appimage.sh").read_text()
        collector = script[script.index("licenses=$appdir/"):script.index("linuxdeploy --output appimage")]
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


if __name__ == "__main__":
    unittest.main()
