"""Analyze real Apple Silicon compiler commands with the macOS SDK."""

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
    def run_command(self, command, succeeds=True):
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode == 0, succeeds, result.stdout + result.stderr)
        return result.stdout + result.stderr

    def test_apple_silicon_analysis(self):
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary)
            (project / "src").mkdir()
            (project / "src/probe.cpp").write_text("int main() { return 0; }\n")
            # Shared code that Linux analyzes; the reduced macOS run must skip its finding.
            (project / "src/shared.cpp").write_text("int shared(int value) { if ((value = 1)) { return value; } return 0; }\n")
            shutil.copyfile(ROOT / ".clang-tidy", project / ".clang-tidy")
            (project / "CMakeLists.txt").write_text("\n".join([
                "cmake_minimum_required(VERSION 3.24)",
                "project(tidy_probe LANGUAGES C CXX)",
                "set(CMAKE_CXX_STANDARD 23)",
                f'include("{ROOT / "cmake/optimization.cmake"}")',
                f'include("{ROOT / "cmake/tidy.cmake"}")',
                "add_executable(probe src/probe.cpp src/shared.cpp)",
            ]) + "\n")
            build = project / "build"
            self.run_command(["cmake", "-S", str(project), "-B", str(build),
                              "-DCMAKE_BUILD_TYPE=MinSizeRel", "-DPERIKOP_LTO=OFF",
                              "-DCMAKE_OSX_ARCHITECTURES=arm64"])
            commands = json.loads((build / "compile_commands.json").read_text())
            self.assertIn("-arch arm64", commands[0]["command"])
            output = self.run_command(["cmake", "--build", str(build), "--target", "tidy"], succeeds=False)
            self.assertIn("bugprone-assignment-in-if-condition", output)
            self.run_command(["cmake", "-S", str(project), "-B", str(build), r"-DORTHO_TIDY_FILES=(src/probe\.cpp)$"])
            self.run_command(["cmake", "--build", str(build), "--target", "tidy"])


if __name__ == "__main__":
    unittest.main()
