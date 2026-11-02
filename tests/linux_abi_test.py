"""Exercise the glibc gate, including dependencies added by packaging tools."""

from pathlib import Path
import sys
import subprocess
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import check_linux_abi


def versions(*names):
    return "Version needs section '.gnu.version_r' contains 1 entry:\n" + "\n".join(
        "  Name: {} Flags: none Version: 2".format(name) for name in names)


class LinuxAbiTest(unittest.TestCase):
    def test_linux_arm64_uses_pi3_cpu_baseline_without_changing_other_targets(self):
        root = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as directory:
            fixture = Path(directory)
            (fixture / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.24)
project(cpu_baseline LANGUAGES NONE)
set(CMAKE_SYSTEM_NAME "${TEST_SYSTEM}")
set(CMAKE_SYSTEM_PROCESSOR "${TEST_CPU}")
set(CMAKE_CXX_COMPILER_ID GNU)
set(CMAKE_SIZEOF_VOID_P 8)
set(APPLE FALSE)
if(TEST_SYSTEM STREQUAL "Darwin")
    set(APPLE TRUE)
endif()
set(PERIKOP_LTO OFF CACHE BOOL "" FORCE)
include("${REPOSITORY}/cmake/optimization.cmake")
get_directory_property(flags COMPILE_OPTIONS)
file(WRITE "${CMAKE_BINARY_DIR}/flags.txt" "${flags}")
''')
            for system, cpu in (("Linux", "aarch64"), ("Linux", "arm64"), ("Linux", "ARM64"),
                                ("Linux", "x86_64"), ("Darwin", "arm64")):
                with self.subTest(system=system, cpu=cpu):
                    build = fixture / f"{system}-{cpu}"
                    result = subprocess.run(["cmake", "-S", str(fixture), "-B", str(build),
                                             f"-DREPOSITORY={root}", f"-DTEST_SYSTEM={system}", f"-DTEST_CPU={cpu}"],
                                            capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    flags = (build / "flags.txt").read_text().split(";")
                    arm_linux = system == "Linux" and cpu != "x86_64"
                    self.assertEqual("-march=armv8-a" in flags, arm_linux)
                    self.assertEqual("-march=x86-64-v2" in flags, cpu == "x86_64")
                    self.assertNotIn("-mcpu=native", flags)
                    self.assertNotIn("-march=native", flags)

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
            with patch("check_linux_abi.subprocess.check_output", return_value=versions("GLIBC_2.39")) as reader:
                check_linux_abi.check([root])
                self.assertEqual(reader.call_count, 2)
            with patch("check_linux_abi.subprocess.check_output",
                       side_effect=[versions("GLIBC_2.39"), versions("GLIBC_2.40")]):
                with self.assertRaisesRegex(ValueError, r"libstdc.*GLIBC_2\.40 \(maximum GLIBC_2\.39\)"):
                    check_linux_abi.check([root])

    def test_rejects_new_and_unknown_requirements(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "perikop"
            binary.write_bytes(b"\x7fELFfake")
            for name in ["GLIBC_2.40", "GLIBC_2.41", "GLIBC_ABI_DT_RELR", "GLIBC_PRIVATE"]:
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
