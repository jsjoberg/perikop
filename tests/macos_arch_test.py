"""A universal executable must not hide a single-architecture bundled library."""

import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("macos_arch", ROOT / "tools/check_macos_arch.py")
macos_arch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(macos_arch)


class MacOSArchitectureTest(unittest.TestCase):
    def fixture(self, root):
        executable = root / "Contents/MacOS/Perikop"
        library = root / "Contents/Frameworks/libonnxruntime.dylib"
        for path in (executable, library):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(bytes.fromhex("cafebabe"))
        return executable, library

    def test_universal_bundle(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.fixture(root)
            (root / "resource.txt").write_text("ordinary resource")
            with patch.object(macos_arch.subprocess, "check_output", return_value="x86_64 arm64\n") as lipo:
                macos_arch.check(root)
                self.assertEqual(lipo.call_count, 2)

    def test_single_architecture_speech_library_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executable, library = self.fixture(root)
            def architectures(command, **kwargs):
                return "arm64\n" if command[-1] == str(library) else "arm64 x86_64\n"
            with patch.object(macos_arch.subprocess, "check_output", side_effect=architectures):
                with self.assertRaisesRegex(ValueError, "libonnxruntime.dylib: expected"):
                    macos_arch.check(root)

    def test_missing_bundle_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "No Mach-O files"):
                macos_arch.check(Path(directory) / "missing.app")


if __name__ == "__main__":
    unittest.main()
