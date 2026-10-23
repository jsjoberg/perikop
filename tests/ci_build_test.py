"""Failure and publication boundaries of the shared CI build entry point."""

from contextlib import chdir
import importlib.util
import os
import re
import sqlite3
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("ci_build", ROOT / "tools/ci/build.py")
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)


class BuildTest(unittest.TestCase):
    def test_publication_requires_every_sanitizer_job(self):
        workflow = (ROOT / ".github/workflows/build.yml").read_text()
        jobs = dict(re.findall(r"^  ([\w-]+):\n(.*?)(?=^  [\w-]+:|\Z)", workflow, re.M | re.S))
        sanitizers = {name for name in jobs if name.endswith("sanitizers")}
        for name in ("publication-check", "release"):
            with self.subTest(job=name):
                job = jobs[name]
                needs = re.search(r"^    needs: \[(.*?)\]", job, re.M)[1].split(", ")
                self.assertTrue(sanitizers <= set(needs))
                # Nightlies may publish partial results; tagged releases must pass every check.
                condition = re.search(r"^    if: (.*)", job, re.M)[1].split(" || (")[0]
                required = set(re.findall(r"needs\.([\w-]+)\.result == 'success'", condition))
                self.assertTrue(sanitizers <= required)
        for name in sanitizers:
            self.assertIn("${{ needs." + name + ".result }}", jobs["release"])

    def test_update_profile_comparison_preserves_values_and_allows_new_tables(self):
        script = (ROOT / "tools/ci/validate_macos_update.command").read_text()
        fixture = re.search(r'"\$database" <<\'SQL\'\n(.*?)\nSQL', script, re.S).group(1)
        snapshot = re.search(r'profile_snapshot\(\) \{.*?<<\'SQL\'\n(.*?)\nSQL', script, re.S).group(1)
        with sqlite3.connect(":memory:") as database:
            database.executescript(fixture)
            def saved_values():
                return [database.execute(query).fetchall()
                        for query in snapshot.split(";") if query.strip()]
            before = saved_values()
            database.execute("CREATE TABLE future_feature(value TEXT)")
            self.assertEqual(saved_values(), before)
            database.execute("UPDATE bookmark SET last_verse=5")
            self.assertNotEqual(saved_values(), before)
            database.execute("UPDATE bookmark SET last_verse=6")
            database.execute("DELETE FROM reading_position")
            self.assertNotEqual(saved_values(), before)

    def test_failed_linux_tests_prevent_packaging(self):
        calls = []
        def run(*arguments, **kwargs):
            calls.append(arguments)
            if "ctest" in arguments:
                raise subprocess.CalledProcessError(1, arguments)
        with tempfile.TemporaryDirectory() as temporary, chdir(temporary), patch.dict(os.environ), \
                patch.object(build, "build"), patch.object(build, "python"), \
                patch.object(build, "run", side_effect=run), patch.object(build.subprocess, "run"):
            Path("build").mkdir()
            with self.assertRaises(subprocess.CalledProcessError):
                build.linux(True)
        self.assertFalse(any("tools/prepare_appdir.sh" in call for call in calls))

    def test_failed_bundle_smoke_check_stops_mac_publication(self):
        def run(*arguments, **kwargs):
            if "--smoke-test" in arguments:
                raise subprocess.CalledProcessError(1, arguments)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with chdir(root), patch.dict(os.environ), patch.object(build, "ROOT", root), \
                    patch.object(build, "build"), patch.object(build, "python"), \
                    patch.object(build, "run", side_effect=run), patch.object(build, "publish_updates") as publish:
                with self.assertRaises(subprocess.CalledProcessError):
                    build.macos(True)
                publish.assert_not_called()

    def test_mac_pull_requests_check_updates_without_staging_a_publication(self):
        with patch.dict(os.environ, GITHUB_EVENT_NAME="pull_request"), patch.object(build, "build"), \
                patch.object(build, "run"), patch.object(build, "python") as python, \
                patch.object(build, "publish_updates") as publish:
            build.macos(False)
            self.assertTrue(any(call.args[0] == "tools/ci/velopack_macos_test.py"
                                for call in python.call_args_list))
            publish.assert_not_called()

    def test_linux_analysis_runs_on_x64_only(self):
        for machine, analyzed in (("x86_64", True), ("aarch64", False)):
            with self.subTest(machine), patch.dict(os.environ), patch.object(build, "build"), \
                    patch.object(build, "python"), patch.object(build, "run") as run, \
                    patch.object(build.platform, "machine", return_value=machine):
                build.linux(False)
                self.assertEqual(any("tidy" in call.args for call in run.call_args_list), analyzed)

    def test_mac_analysis_covers_only_apple_specific_sources(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "src").mkdir()
            (root / "src/shared.cpp").write_text("int shared();\n")
            (root / "src/menus.cpp").write_text("#ifdef __WXOSX__\n#endif\n")
            (root / "src/native.mm").write_text("")
            with patch.dict(os.environ), patch.object(build, "ROOT", root), patch.object(build, "build") as configure, \
                    patch.object(build, "run"), patch.object(build, "python"):
                build.macos(False)
            files = next(option for option in configure.call_args.args if option.startswith("-DORTHO_TIDY_FILES="))
        pattern = files.split("=", 1)[1]
        self.assertTrue(re.search(pattern, "/work/src/menus.cpp"))
        self.assertTrue(re.search(pattern, "/work/src/native.mm"))
        self.assertFalse(re.search(pattern, "/work/src/shared.cpp"))

    def test_linux_sanitizers_run_every_test_on_a_virtual_display(self):
        with patch.dict(os.environ), patch.object(build, "build") as configure, patch.object(build, "run") as run:
            build.linux_sanitizers()
        self.assertIn("-DORTHO_SANITIZERS=ON", configure.call_args.args)
        ctest = run.call_args.args
        self.assertEqual(ctest[:len(build.DISPLAY) + 1], (*build.DISPLAY, "ctest"))
        self.assertNotIn("-L", ctest)


if __name__ == "__main__":
    unittest.main()
