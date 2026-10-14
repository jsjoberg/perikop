"""Analyze real macOS compiler commands without losing either build architecture."""

import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
TOOLS = all(shutil.which(name) for name in ("cmake", "clang-tidy", "run-clang-tidy"))


@unittest.skipUnless(sys.platform == "darwin" and TOOLS, "Requires macOS and the LLVM analysis tools")
class MacOSTidyTest(unittest.TestCase):
    def run_command(self, command):
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_universal_and_single_architecture_analysis(self):
        cases = [(architectures, host) for architectures in ("arm64;x86_64", "x86_64;arm64")
                 for host in ("arm64", "x86_64")]
        cases += [("arm64", None), ("x86_64", None)]
        for architectures, host in cases:
            with self.subTest(architectures=architectures, host=host), tempfile.TemporaryDirectory() as temporary:
                project = Path(temporary)
                (project / "src").mkdir()
                (project / "src/probe.cpp").write_text("int main() { return 0; }\n")
                shutil.copyfile(ROOT / ".clang-tidy", project / ".clang-tidy")
                (project / "CMakeLists.txt").write_text("\n".join([
                    "cmake_minimum_required(VERSION 3.24)",
                    "project(tidy_probe LANGUAGES C CXX)",
                    "set(CMAKE_CXX_STANDARD 23)",
                    f'include("{ROOT / "cmake/optimization.cmake"}")',
                    # Exercise each analysis slice, including Intel selection on an ARM computer.
                    f"set(CMAKE_HOST_SYSTEM_PROCESSOR {host})" if host else "",
                    f'include("{ROOT / "cmake/tidy.cmake"}")',
                    "add_executable(probe src/probe.cpp)",
                ]) + "\n")
                build = project / "build"
                self.run_command(["cmake", "-S", str(project), "-B", str(build),
                                  "-DCMAKE_BUILD_TYPE=MinSizeRel", "-DPERIKOP_LTO=OFF",
                                  "-DCMAKE_OSX_ARCHITECTURES=" + architectures])
                commands = json.loads((build / "compile_commands.json").read_text())
                command = commands[0]["command"]
                for architecture in architectures.split(";"):
                    self.assertIn("-arch " + architecture, command)
                if "x86_64" in architectures:
                    self.assertIn("-Xarch_x86_64 -march=x86-64-v2", command)
                    self.assertIn("-Xarch_x86_64 -mtune=generic", command)
                self.run_command(["cmake", "--build", str(build), "--target", "tidy"])


if __name__ == "__main__":
    unittest.main()
