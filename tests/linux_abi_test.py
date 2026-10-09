"""Exercise the glibc gate, including dependencies added by packaging tools."""

from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import check_linux_abi


def versions(*names):
    return "Version needs section '.gnu.version_r' contains 1 entry:\n" + "\n".join(
        "  Name: {} Flags: none Version: 2".format(name) for name in names)


class LinuxAbiTest(unittest.TestCase):
    def test_ignores_definitions_and_cpp_versions(self):
        output = "Version definition section '.gnu.version_d':\n  Name: GLIBC_2.40\n"
        output += versions("GLIBC_2.2.5", "GLIBC_2.34", "GLIBCXX_3.4.35", "CXXABI_1.3.16")
        self.assertEqual(check_linux_abi.glibc_requirements(output), {"GLIBC_2.2.5", "GLIBC_2.34"})

    def test_scans_nested_libraries_and_skips_other_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "perikop").write_bytes(b"\x7fELFfake")
            (root / "usr/lib").mkdir(parents=True)
            library = root / "usr/lib/libstdc++.so.6"
            library.write_bytes(b"\x7fELFfake")
            (root / "icon.png").write_bytes(b"PNG")
            with patch("check_linux_abi.subprocess.check_output", return_value=versions("GLIBC_2.34")) as reader:
                check_linux_abi.check([root])
                self.assertEqual(reader.call_count, 2)
            with patch("check_linux_abi.subprocess.check_output", side_effect=[versions("GLIBC_2.34"), versions("GLIBC_2.35")]):
                with self.assertRaisesRegex(ValueError, "libstdc.*GLIBC_2.35"):
                    check_linux_abi.check([root])

    def test_rejects_new_and_unknown_requirements(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "perikop"
            binary.write_bytes(b"\x7fELFfake")
            for name in ["GLIBC_2.35", "GLIBC_2.39", "GLIBC_ABI_DT_RELR", "GLIBC_PRIVATE"]:
                with self.subTest(name=name), patch("check_linux_abi.subprocess.check_output", return_value=versions(name)):
                    with self.assertRaisesRegex(ValueError, name):
                        check_linux_abi.check([binary])

    def test_rejects_empty_input_and_readelf_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaisesRegex(ValueError, "No ELF"):
                check_linux_abi.check([root])
            binary = root / "perikop"
            binary.write_bytes(b"\x7fELFfake")
            with patch("check_linux_abi.subprocess.check_output", side_effect=FileNotFoundError):
                with self.assertRaises(FileNotFoundError):
                    check_linux_abi.check([root])


if __name__ == "__main__":
    unittest.main()
