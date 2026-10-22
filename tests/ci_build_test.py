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


if __name__ == "__main__":
    unittest.main()
