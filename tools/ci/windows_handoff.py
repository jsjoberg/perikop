#!/usr/bin/env python3
"""Export CTest commands without Linux paths for the compiler-free Windows job."""

import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess


def export(source, build):
    source, build = source.resolve(), build.resolve()
    header = (build / "generated/perikop_version.hpp").read_text()
    version = re.search(r'#define PERIKOP_VERSION "([^"]+)"', header)
    numbers = re.search(r'#define PERIKOP_VERSION_NUMBERS (\d+),(\d+),(\d+),0', header)
    if not version or not numbers:
        raise ValueError("Prebuilt version header is invalid")
    cmake = Path(shutil.which("cmake"))
    cmake_paths = {str(cmake), str(cmake.resolve())}

    def portable(value):
        if isinstance(value, list):
            return [portable(item) for item in value]
        if not isinstance(value, str):
            return value
        if value in cmake_paths:
            return "@CMAKE@"
        return value.replace(str(build), "@BUILD@").replace(str(source), "@SOURCE@")

    result = subprocess.run(["ctest", "--test-dir", str(build), "--show-only=json-v1"],
                            capture_output=True, text=True, check=True)
    tests = json.loads(result.stdout)["tests"]
    if not tests:
        raise ValueError("Windows handoff contains no tests")
    for test in tests:
        test.pop("backtrace", None)
        if not test.get("command"):
            raise ValueError("Prebuilt test has no command: " + test["name"])
        test["command"] = portable(test["command"])
        for argument in test["command"]:
            if argument.startswith("/") or re.match(r"-D[^=]+=/", argument):
                raise ValueError("Prebuilt test retains a host path: " + argument)
        for prop in test.get("properties", []):
            prop["value"] = portable(prop["value"])
            if prop["name"] == "WORKING_DIRECTORY" and prop["value"].startswith("/"):
                raise ValueError("Prebuilt test retains a host working directory: " + prop["value"])
    manifest = {"schema": 1, "project_version": ".".join(numbers.groups()),
                "version": version[1], "tests": tests}
    (build / "windows-handoff.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    args = parser.parse_args()
    export(args.source, args.build)
