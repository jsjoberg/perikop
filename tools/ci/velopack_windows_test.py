#!/usr/bin/env python3
"""Install Perikop with Velopack's Setup.exe, let it update itself from a local feed, and uninstall it.

Run it on Windows with the bin folder of an installed build (cmake --install). It packs that folder as
two nightly versions with vpk, installs the first silently for the current user, and checks that:
- the installed program updates itself with --update-probe, the same backend as Inställningar,
- the new version reads the same profile (its channel decides which feed is read) and leaves it as it was,
- uninstalling removes the program and keeps the profile.
"""
import argparse
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
VPK = ROOT / "build/ci/tools/vpk/tools/net8.0/any/vpk.dll"
FIRST, SECOND = "1.0.0-test.1", "1.0.0-test.2"
CHANNEL = "win-x64-nightly"


def run(arguments, timeout=300):
    result = subprocess.run([str(value) for value in arguments], text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=timeout)
    return result.returncode, result.stdout


def settings(database):
    with sqlite3.connect(database) as connection:
        return sorted(connection.execute("SELECT key, value FROM settings"))


def wait_for(condition, what, seconds=120):
    deadline = time.monotonic() + seconds
    while not condition():
        if time.monotonic() > deadline:
            raise SystemExit("Timed out: " + what)
        time.sleep(2)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("bin", type=Path, help="The bin folder of an installed build")
    args = parser.parse_args()
    installation = Path(os.environ["LOCALAPPDATA"]) / "Perikop"
    if installation.exists():
        raise SystemExit(f"{installation} already exists; this test needs a machine without Perikop installed")
    with tempfile.TemporaryDirectory(prefix="perikop-velopack-") as temporary:
        work = Path(temporary)
        feed, profile = work / "feed", work / "profile"
        profile.mkdir()
        setup = work / "Setup.exe"
        for version in (FIRST, SECOND):
            code, output = run(["dotnet", VPK, "pack", "--packId", "Perikop", "--packTitle", "Perikop",
                                "--packVersion", version, "--packDir", args.bin, "--mainExe", "perikop.exe",
                                "--runtime", "win-x64", "--channel", CHANNEL, "--delta", "None",
                                "--outputDir", feed], 600)
            if code:
                raise SystemExit(f"Packing {version} failed:\n{output}")
            if version == FIRST:
                shutil.copyfile(feed / f"Perikop-{CHANNEL}-Setup.exe", setup)
        # The profile asks for nightlies; only the nightly feed exists, so a copy that ignored it would fail.
        database = profile / "user.db"
        with sqlite3.connect(database) as connection:
            connection.execute("CREATE TABLE settings(key TEXT PRIMARY KEY NOT NULL,value TEXT NOT NULL) STRICT")
            connection.execute("CREATE TABLE progress(key TEXT PRIMARY KEY NOT NULL,completed TEXT NOT NULL) STRICT")
            connection.executemany("INSERT INTO settings VALUES(?,?)",
                                   [("update_channel", "nightly"), ("theme", "dark"), ("color_scheme", "nord")])
        before = settings(database)

        code, output = run([setup, "--silent"], 600)
        program = installation / "current/perikop.exe"
        if code or not program.is_file():
            raise SystemExit(f"Silent installation failed ({code}):\n{output}")

        def probe():
            return run([program, "--profile", profile, "--update-probe", feed])

        code, output = probe()
        if code or f"VERSION {FIRST}\n" not in output or f"AVAILABLE {SECOND}\n" not in output \
                or "APPLYING\n" not in output:
            raise SystemExit(f"The first version did not start the update ({code}):\n{output}")
        # Update.exe replaces the program once it has exited.
        updated = {}

        def replaced():
            updated["code"], updated["output"] = probe()
            return updated["code"] == 0 and f"VERSION {SECOND}\n" in updated["output"]

        wait_for(replaced, "the installed program was not replaced")
        if "NO_UPDATE\n" not in updated["output"]:
            raise SystemExit(f"The updated version was offered an update again:\n{updated['output']}")
        if settings(database) != before:
            raise SystemExit(f"The update changed the profile: {before} became {settings(database)}")

        code, output = run([installation / "Update.exe", "--uninstall", "--silent"], 600)
        if code:
            raise SystemExit(f"Uninstalling failed ({code}):\n{output}")
        wait_for(lambda: not program.exists(), "uninstalling left the program in place")
        if settings(database) != before:
            raise SystemExit("Uninstalling changed the profile")
    print(f"Installed {FIRST}, updated it to {SECOND} on {CHANNEL}, and uninstalled it; profile kept")


if __name__ == "__main__":
    main()
