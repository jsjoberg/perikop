#!/usr/bin/env python3
"""Update a Velopack-installed Perikop from a local feed, as Inställningar does, and check the result.

Run it on the glibc 2.34 baseline after tools/package_appimage.sh, with LD_PRELOAD naming a SQLite that
reads STRICT tables (see tools/ci/build-linux.sh). It packs the same build as two nightly versions,
installs the first, and lets the program update itself with --update-probe. It checks that:
- the update applies and replaces the installed AppImage,
- the profile stays as it was, and the new version reads it (its channel decides which feed is read),
- the program opens no Internet socket.
"""
import argparse
from contextlib import closing
import hashlib
import os
from pathlib import Path
import platform
import re
import shutil
import sqlite3
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
FIRST, SECOND, THIRD = "1.0.0-test.1", "1.0.0-test.2", "1.0.0-test.3"


def run(arguments, environment, timeout=120):
    result = subprocess.run([str(value) for value in arguments], env=environment, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)
    return result.returncode, result.stdout


def profile_contents(database):
    with closing(sqlite3.connect(database)) as connection:
        return (sorted(connection.execute("SELECT key, value FROM settings")),
                sorted(connection.execute("SELECT key, completed FROM progress")))


def image_hash(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def wait_for_image(installed, expected):
    deadline = time.monotonic() + 120
    while True:
        try:
            if image_hash(installed) == expected:
                return
        except OSError:
            pass
        if time.monotonic() > deadline:
            raise SystemExit("The update did not replace the installed AppImage")
        time.sleep(2)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("build", type=Path, help="The build directory whose AppDir tools/package_appimage.sh made")
    args = parser.parse_args()
    architecture = {"x86_64": "x64", "aarch64": "arm64"}[platform.machine()]
    channel = f"linux-{architecture}-nightly"
    with tempfile.TemporaryDirectory(prefix="perikop-velopack-") as temporary:
        work = Path(temporary)
        feed, profile, home = work / "feed", work / "profile", work / "home"
        for directory in (feed, profile, home):
            directory.mkdir()
        # The program's own processes get the system SQLite, a private home, and no FUSE.
        child = {key: value for key, value in os.environ.items() if key != "LD_PRELOAD"}
        child.update(HOME=str(home), XDG_DATA_HOME=str(home / "data"), XDG_CACHE_HOME=str(home / "cache"),
                     XDG_CONFIG_HOME=str(home / "config"), APPIMAGE_EXTRACT_AND_RUN="1")
        installed = work / "installed/Perikop.AppImage"
        installed.parent.mkdir()
        for version in (FIRST, SECOND):
            code, output = run([ROOT / "tools/package_velopack.sh", args.build, version, channel, feed], child, 600)
            if code:
                raise SystemExit(f"Packing {version} failed:\n{output}")
            if version == FIRST:
                images = list(feed.glob("*.AppImage"))
                if len(images) != 1:
                    raise SystemExit(f"Expected one AppImage, found {images}")
                shutil.copyfile(images[0], installed)
                installed.chmod(0o755)
        # The profile asks for nightlies; only the nightly feed exists, so a copy that ignored it would fail.
        database = profile / "user.db"
        with closing(sqlite3.connect(database)) as connection, connection:
            connection.execute("CREATE TABLE settings(key TEXT PRIMARY KEY NOT NULL,value TEXT NOT NULL) STRICT")
            connection.execute("CREATE TABLE progress(key TEXT PRIMARY KEY NOT NULL,completed TEXT NOT NULL) STRICT")
            connection.execute("INSERT INTO progress VALUES('update-test','2026-10-18')")
            connection.executemany("INSERT INTO settings VALUES(?,?)",
                                   [("update_channel", "nightly"), ("theme", "dark"), ("color_scheme", "nord")])
        before = profile_contents(database)
        expected_hash = image_hash(next(feed.glob("*.AppImage")))

        def probe(name):
            trace = work / (name + ".trace")
            code, output = run(["xvfb-run", "-a", "strace", "-f", "-e", "trace=network", "-o", trace,
                                installed, "--profile", profile, "--update-probe", feed], child)
            sockets = re.findall(r"socket\((AF_INET6?)", trace.read_text())
            if sockets:
                raise SystemExit(f"{name}: opened {sorted(set(sockets))} sockets; see {trace}")
            return code, output

        code, output = probe("first")
        if code or f"VERSION {FIRST}\n" not in output or f"AVAILABLE {SECOND}\n" not in output \
                or "APPLYING\n" not in output:
            raise SystemExit(f"The first version did not start the update:\n{output}")
        # UpdateNix replaces the AppImage once the program has exited.
        wait_for_image(installed, expected_hash)
        code, output = probe("updated")
        if code or f"VERSION {SECOND}\n" not in output or "NO_UPDATE\n" not in output:
            raise SystemExit(f"The full-updated version failed:\n{output}")
        delta_child = dict(child, PERIKOP_VELOPACK_DELTA="BestSpeed")
        code, output = run([ROOT / "tools/package_velopack.sh", args.build, THIRD, channel, feed], delta_child, 600)
        if code:
            raise SystemExit(f"Packing the delta failed:\n{output}")
        if not (feed / f"Perikop-{THIRD}-{channel}-delta.nupkg").is_file():
            raise SystemExit("The third version has no delta package")
        (feed / f"Perikop-{THIRD}-{channel}-full.nupkg").rename(work / "third-full.nupkg")
        image = next(feed.glob("*.AppImage"))
        expected_hash = image_hash(image)
        code, output = probe("delta-start")
        if code or f"AVAILABLE {THIRD}\n" not in output or "APPLYING\n" not in output:
            raise SystemExit(f"The delta update failed without its full package:\n{output}")
        wait_for_image(installed, expected_hash)
        code, output = probe("delta-updated")
        if code or f"VERSION {THIRD}\n" not in output or "NO_UPDATE\n" not in output:
            raise SystemExit(f"The delta-updated version failed:\n{output}")
        if profile_contents(database) != before:
            raise SystemExit(f"The update changed the profile: {before} became {profile_contents(database)}")
    print(f"Full-updated {FIRST} to {SECOND}, delta-updated to {THIRD} without its full package on {channel}: "
          "profile kept, no Internet sockets")


if __name__ == "__main__":
    main()
