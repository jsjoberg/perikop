"""Check the cross-build handoff without a Windows host or a downloaded compiler."""

from contextlib import chdir
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("windows_handoff", ROOT / "tools/ci/windows_handoff.py")
handoff = importlib.util.module_from_spec(spec)
spec.loader.exec_module(handoff)
build_spec = importlib.util.spec_from_file_location("ci_build", ROOT / "tools/ci/build.py")
builder = importlib.util.module_from_spec(build_spec)
build_spec.loader.exec_module(builder)


class WindowsCrossTest(unittest.TestCase):
    def fixture(self, root):
        producer = root / "producer"
        producer.mkdir()
        (producer / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.24)
project(handoff LANGUAGES NONE)
enable_testing()
add_test(NAME prebuilt COMMAND "${CMAKE_BINARY_DIR}/bin/ortho-tests.exe"
    "${REPOSITORY}/resources" "one;argument" "-DPROFILE_DIR=${CMAKE_BINARY_DIR}/test profile")
set_tests_properties(prebuilt PROPERTIES LABELS "gui;handoff" TIMEOUT 17
    ENVIRONMENT "HANDOFF_ENV=fixture value" WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
add_test(NAME build-version COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${REPOSITORY}"
    "-DWORK_DIR=${CMAKE_BINARY_DIR}/test-version" -P "${REPOSITORY}/tests/build_version_test.cmake")
''')
        build = root / "producer build"
        self.run_command(["cmake", "-S", str(producer), "-B", str(build), "-G", "Ninja",
                          "-DREPOSITORY=" + str(ROOT)])
        (build / "bin/resources").mkdir(parents=True)
        (build / "bin/resources/LICENSE").write_text("fixture license")
        (build / "bin/onnxruntime.dll").write_bytes(b"upstream runtime")
        (build / "bin/velopack_libc.dll").write_bytes(b"update sdk")
        (build / "bin/perikop.exe").write_text("#!/bin/sh\nexit 0\n")
        binary = build / "bin/ortho-tests.exe"
        binary.write_text('''#!/bin/sh
set -eu
test -d "$1"
test "$2" = 'one;argument'
test "$3" = "-DPROFILE_DIR=$PWD/test profile"
test "$HANDOFF_ENV" = 'fixture value'
''')
        for path in (build / "bin").glob("*.exe"):
            path.chmod(0o755)
        version = re.search(r'project\(perikop VERSION ([0-9.]+)', (ROOT / "CMakeLists.txt").read_text())[1]
        self.run_command(["cmake", "-DSOURCE_DIR=" + str(ROOT), "-DVERSION=" + version,
                          "-DOUTPUT=" + str(build / "generated/perikop_version.hpp"),
                          "-P", str(ROOT / "cmake/version.cmake")])
        handoff.export(ROOT, build)
        # Transfer only the portable files into a new build directory.
        consumer = root / "consumer build"
        shutil.copytree(build / "bin", consumer / "bin")
        shutil.copytree(build / "generated", consumer / "generated")
        shutil.copyfile(build / "windows-handoff.json", consumer / "windows-handoff.json")
        runtime = root / "CRT"
        runtime.mkdir()
        for name in ("msvcp140", "msvcp140_1", "vcruntime140", "vcruntime140_1"):
            (runtime / (name + ".dll")).write_bytes(b"fixture CRT")
        return consumer, runtime

    def run_command(self, command, **kwargs):
        result = subprocess.run(command, capture_output=True, text=True, **kwargs)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def configure(self, build, runtime, **kwargs):
        return subprocess.run(["cmake", "-S", str(ROOT / "cmake/windows-prebuilt"), "-B", str(build),
                               "-G", "Ninja", "-DCMAKE_SYSTEM_NAME=Windows",
                               "-DPERIKOP_MSVC_RUNTIME=" + str(runtime)], capture_output=True, text=True, **kwargs)

    def test_native_configuration_runs_and_packages_without_any_compiler(self):
        with tempfile.TemporaryDirectory() as temporary:
            build, runtime = self.fixture(Path(temporary).resolve())
            env = dict(os.environ, CC="/must-not-run-a-compiler", CXX="/must-not-run-a-compiler", COPYFILE_DISABLE="1")
            result = self.configure(build, runtime, env=env)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertFalse(list((build / "CMakeFiles").glob("*/CMake*C*Compiler.cmake")))
            self.run_command(["ctest", "--test-dir", str(build), "--output-on-failure"])
            tests = json.loads(self.run_command(["ctest", "--test-dir", str(build),
                                                "--show-only=json-v1"]).stdout)["tests"]
            prebuilt = next(test for test in tests if test["name"] == "prebuilt")
            properties = {prop["name"]: prop["value"] for prop in prebuilt["properties"]}
            self.assertEqual(properties["LABELS"], ["gui", "handoff"])
            self.assertEqual(properties["TIMEOUT"], 17)
            self.run_command(["cpack", "--config", str(build / "CPackConfig.cmake"), "-G", "TGZ",
                              "-B", str(build / "package")], env=env)
            packages = list((build / "package").glob("*.tar.gz"))
            self.assertEqual(len(packages), 1)
            with tarfile.open(packages[0]) as archive:
                # macOS CPack adds AppleDouble records. Windows and Linux do not.
                files = {member.name.split("/", 1)[1] for member in archive if member.isfile()
                         and (sys.platform != "darwin" or not Path(member.name).name.startswith("._"))}
                self.assertEqual(files, {"bin/perikop.exe", "bin/onnxruntime.dll", "bin/velopack_libc.dll",
                                         "bin/resources/LICENSE",
                                         "bin/msvcp140.dll", "bin/msvcp140_1.dll", "bin/vcruntime140.dll",
                                         "bin/vcruntime140_1.dll"})
                runtime_member = next(member for member in archive if member.name.endswith("/bin/onnxruntime.dll"))
                self.assertEqual(archive.extractfile(runtime_member).read(), b"upstream runtime")

    def test_prebuilt_configuration_rejects_missing_runtime(self):
        with tempfile.TemporaryDirectory() as temporary:
            build, runtime = self.fixture(Path(temporary).resolve())
            (runtime / "msvcp140_1.dll").unlink()
            result = self.configure(build, runtime)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Visual C++ runtime file is missing: msvcp140_1.dll", result.stderr)

    def test_prebuilt_configuration_rejects_wrong_version(self):
        with tempfile.TemporaryDirectory() as temporary:
            build, runtime = self.fixture(Path(temporary).resolve())
            manifest = build / "windows-handoff.json"
            data = json.loads(manifest.read_text())
            data["version"] = "wrong-version"
            manifest.write_text(json.dumps(data))
            result = self.configure(build, runtime)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("does not match source version", result.stderr)

    def test_prebuilt_configuration_rejects_missing_test_binary(self):
        with tempfile.TemporaryDirectory() as temporary:
            build, runtime = self.fixture(Path(temporary).resolve())
            (build / "bin/ortho-tests.exe").unlink()
            result = self.configure(build, runtime)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Prebuilt test executable is missing", result.stderr)

    def test_export_rejects_host_only_paths_and_empty_test_lists(self):
        with tempfile.TemporaryDirectory() as temporary:
            build, runtime = self.fixture(Path(temporary).resolve())
            cases = [([], "contains no tests"),
                     ([{"name": "host", "command": ["/unmapped/host-tool"], "properties": []}], "retains a host path"),
                     ([{"name": "host", "command": ["cmake"], "properties": [
                         {"name": "WORKING_DIRECTORY", "value": "/unmapped/workdir"}]}], "host working directory")]
            for tests, error in cases:
                with self.subTest(error=error):
                    result = subprocess.CompletedProcess([], 0, stdout=json.dumps({"tests": tests}))
                    with patch.object(handoff.subprocess, "run", return_value=result):
                        with self.assertRaisesRegex(ValueError, error):
                            handoff.export(ROOT, build)

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
    printf 'update sdk' > build/cmake/bin/velopack_libc.dll
    printf 'resources' > build/cmake/bin/resources/fixture
    printf 'version' > build/cmake/generated/perikop_version.hpp
    printf 'linux paths' > build/cmake/CMakeCache.txt
    printf 'linux tests' > build/cmake/CTestTestfile.cmake
    printf 'linux install' > build/cmake/cmake_install.cmake
fi
''')
            (tools / "ccache").write_text("#!/bin/bash\nexit 0\n")
            (tools / "llvm-strip").write_text('#!/bin/bash\necho "$*" >> strip.log\n')
            for tool in tools.iterdir():
                tool.chmod(0o755)
            env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ["PATH"])
            def handoff_probe(*arguments):
                (root / "build/cmake/windows-handoff.json").write_text("manifest")
            with chdir(root), patch.dict(os.environ, env), patch.object(builder, "ROOT", root), \
                    patch.object(builder, "SOURCES", root / "build/ci/sources"), \
                    patch.object(builder, "python", side_effect=handoff_probe) as probe:
                builder.windows()
                probe.assert_called_once_with("tools/ci/windows_handoff.py", "--source", ".", "--build", Path("build/cmake"))
            commands = (root / "commands.log").read_text().splitlines()
            self.assertEqual(len(commands), 2)
            self.assertIn("--toolchain cmake/llvm-mingw.cmake", commands[0])
            self.assertIn(f"-DPERIKOP_DEPENDENCY_DIR={root}/build/ci/sources", commands[0])
            self.assertEqual(commands[1], "--build build/cmake --parallel 4")
            self.assertEqual((root / "strip.log").read_text().strip(), "build/cmake/bin/perikop.exe")
            with tarfile.open(root / "build/windows-x64.tar.gz") as archive:
                files = {member.name for member in archive if member.isfile()}
            self.assertEqual(files, {"bin/perikop.exe", "bin/ortho-tests.exe", "bin/onnxruntime.dll", "bin/velopack_libc.dll",
                                     "bin/resources/fixture", "generated/perikop_version.hpp", "windows-handoff.json"})

    def test_toolchain_rejects_unknown_architecture(self):
        result = subprocess.run(["cmake", "-DPERIKOP_WINDOWS_ARCH=unknown", "-P",
                                 str(ROOT / "cmake/llvm-mingw.cmake")], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Unsupported Windows architecture", result.stderr)


if __name__ == "__main__":
    unittest.main()
