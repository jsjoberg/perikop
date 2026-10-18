#!/usr/bin/env python3
"""Install Perikop with Velopack's Setup.exe, let it update itself from a local feed, and uninstall it.

Run it on Windows with the bin folder of an installed build (cmake --install). It packs that folder as
two nightly versions with vpk, installs the first silently for the current user, and checks that:
- the installed program updates itself with --update-probe, the same backend as Inställningar,
- the new version reads the same profile (its channel decides which feed is read) and leaves it as it was,
- uninstalling removes the program and keeps the profile.
"""
import argparse
from contextlib import closing
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess
import tempfile
import time
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
VPK = ROOT / "build/ci/tools/vpk/tools/net8.0/any/vpk.dll"
FIRST, SECOND, THIRD = "1.0.0-test.1", "1.0.0-test.2", "1.0.0-test.3"
CHANNEL = "win-x64-nightly"


def run(arguments, timeout=300):
    result = subprocess.run([str(value) for value in arguments], text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=timeout)
    return result.returncode, result.stdout


def profile_contents(database):
    with closing(sqlite3.connect(database)) as connection:
        return (sorted(connection.execute("SELECT key, value FROM settings")),
                sorted(connection.execute("SELECT key, completed FROM progress")))


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
        def pack(version, delta="None"):
            code, output = run(["dotnet", VPK, "pack", "--packId", "Perikop", "--packTitle", "Perikop",
                                "--packVersion", version, "--packDir", args.bin, "--mainExe", "perikop.exe",
                                "--runtime", "win-x64", "--channel", CHANNEL, "--delta", delta,
                                "--outputDir", feed], 600)
            if code:
                raise SystemExit(f"Packing {version} failed:\n{output}")
        for version in (FIRST, SECOND):
            pack(version)
            if version == FIRST:
                shutil.copyfile(feed / f"Perikop-{CHANNEL}-Setup.exe", setup)
        # The profile asks for nightlies; only the nightly feed exists, so a copy that ignored it would fail.
        database = profile / "user.db"
        # The connection context commits the fixture; closing releases the Windows file handle.
        with closing(sqlite3.connect(database)) as connection, connection:
            connection.execute("CREATE TABLE settings(key TEXT PRIMARY KEY NOT NULL,value TEXT NOT NULL) STRICT")
            connection.execute("CREATE TABLE progress(key TEXT PRIMARY KEY NOT NULL,completed TEXT NOT NULL) STRICT")
            connection.execute("INSERT INTO progress VALUES('update-test','2026-10-18')")
            connection.executemany("INSERT INTO settings VALUES(?,?)",
                                   [("update_channel", "nightly"), ("theme", "dark"), ("color_scheme", "nord")])
        before = profile_contents(database)

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
        def installed_version():
            try:
                return next(element.text for element in ET.parse(installation / "current/sq.version").iter()
                            if element.tag.rsplit("}", 1)[-1] == "version")
            except (OSError, ET.ParseError, StopIteration):
                return ""

        wait_for(lambda: installed_version() == SECOND, "the installed program was not replaced")
        code, output = probe()
        if code or f"VERSION {SECOND}\n" not in output or "NO_UPDATE\n" not in output:
            raise SystemExit(f"The updated version failed ({code}):\n{output}")
        pack(THIRD, "BestSpeed")
        delta = feed / f"Perikop-{THIRD}-{CHANNEL}-delta.nupkg"
        if not delta.is_file():
            raise SystemExit("The third version has no delta package")
        # The SDK cached SECOND during its full upgrade. Remove THIRD's full package from the
        # feed so this step can succeed only by applying the delta to that cached base.
        (feed / f"Perikop-{THIRD}-{CHANNEL}-full.nupkg").rename(work / "third-full.nupkg")
        code, output = probe()
        if code or f"AVAILABLE {THIRD}\n" not in output or "APPLYING\n" not in output:
            raise SystemExit(f"The delta update failed ({code}):\n{output}")
        wait_for(lambda: installed_version() == THIRD, "the delta update was not installed")
        code, output = probe()
        if code or f"VERSION {THIRD}\n" not in output or "NO_UPDATE\n" not in output:
            raise SystemExit(f"The delta-updated version failed ({code}):\n{output}")
        if profile_contents(database) != before:
            raise SystemExit(f"The update changed the profile: {before} became {profile_contents(database)}")

        code, output = run([installation / "Update.exe", "--uninstall", "--silent"], 600)
        if code:
            raise SystemExit(f"Uninstalling failed ({code}):\n{output}")
        wait_for(lambda: not program.exists(), "uninstalling left the program in place")
        if profile_contents(database) != before:
            raise SystemExit("Uninstalling changed the profile")
    print(f"Installed {FIRST}, full-updated to {SECOND}, delta-updated to {THIRD} without its full package, "
          f"and uninstalled it on {CHANNEL}; profile kept")


if __name__ == "__main__":
    main()
