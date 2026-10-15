"""Check the cross-build handoff without a Windows host or a downloaded compiler."""

import os
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class WindowsCrossTest(unittest.TestCase):
    def test_native_configuration_runs_and_packages_prebuilt_targets_without_building(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            # A target that cannot compile makes an accidental rebuild fail this test.
            (root / "probe.c").write_text('#error "This target must not rebuild"\n')
            (root / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.24)
project(handoff VERSION 1.0.0 LANGUAGES C)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
enable_testing()
add_executable(probe probe.c)
set_target_properties(probe PROPERTIES SUFFIX ".exe")
add_test(NAME prebuilt COMMAND probe)
install(TARGETS probe RUNTIME DESTINATION bin)
set(CPACK_GENERATOR TGZ)
include(CPack)
''')
            build = root / "build"
            subprocess.run(["cmake", "-S", str(root), "-B", str(build), "-G", "Ninja"],
                           check=True, capture_output=True, text=True)
            binary = build / "bin/probe.exe"
            binary.parent.mkdir(exist_ok=True)
            binary.write_text("#!/bin/sh\nexit 0\n")
            binary.chmod(0o755)
            subprocess.run(["ctest", "--test-dir", str(build), "--output-on-failure"],
                           check=True, capture_output=True, text=True)
            subprocess.run(["cpack", "--config", str(build / "CPackConfig.cmake"),
                            "-B", str(build / "package")], check=True, capture_output=True, text=True)
            packages = list((build / "package").glob("*.tar.gz"))
            self.assertEqual(len(packages), 1)
            with tarfile.open(packages[0]) as archive:
                self.assertTrue(any(member.name.endswith("/bin/probe.exe") for member in archive))

    def test_archive_contains_binaries_and_version_but_no_host_configuration(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            tools = root / "tools"
            tools.mkdir()
            # Stand in for the compiler so this test checks only the transfer boundary.
            (tools / "cmake").write_text('''#!/bin/bash
set -eu
echo "$*" >> commands.log
if [ "$1" = --build ]; then
    mkdir -p build/cmake/bin/resources build/cmake/generated
    printf 'app' > build/cmake/bin/perikop.exe
    printf 'test' > build/cmake/bin/ortho-tests.exe
    printf 'runtime' > build/cmake/bin/onnxruntime.dll
    printf 'resources' > build/cmake/bin/resources/fixture
    printf 'version' > build/cmake/generated/perikop_version.hpp
    printf 'linux paths' > build/cmake/CMakeCache.txt
    printf 'linux tests' > build/cmake/CTestTestfile.cmake
    printf 'linux install' > build/cmake/cmake_install.cmake
fi
''')
            (tools / "ccache").write_text("#!/bin/bash\nexit 0\n")
            for tool in tools.iterdir():
                tool.chmod(0o755)
            env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ["PATH"])
            subprocess.run(["bash", str(ROOT / "tools/ci/build-windows-cross.sh")],
                           cwd=root, env=env, check=True, capture_output=True, text=True)
            commands = (root / "commands.log").read_text().splitlines()
            self.assertEqual(len(commands), 2)
            self.assertIn("--toolchain cmake/llvm-mingw.cmake", commands[0])
            self.assertIn("-DPERIKOP_DEPENDENCY_DIR=/work/build/ci/sources", commands[0])
            self.assertEqual(commands[1], "--build build/cmake --parallel 4")
            with tarfile.open(root / "build/windows-x64.tar.gz") as archive:
                files = {member.name for member in archive if member.isfile()}
            self.assertEqual(files, {"bin/perikop.exe", "bin/ortho-tests.exe", "bin/onnxruntime.dll",
                                     "bin/resources/fixture", "generated/perikop_version.hpp"})

    def test_toolchain_rejects_unknown_architecture(self):
        result = subprocess.run(["cmake", "-DPERIKOP_WINDOWS_ARCH=unknown", "-P",
                                 str(ROOT / "cmake/llvm-mingw.cmake")], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Unsupported Windows architecture", result.stderr)


if __name__ == "__main__":
    unittest.main()
