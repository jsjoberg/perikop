#!/usr/bin/env python3
"""Explicit, isolated Velopack research; never used by ordinary builds or releases."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import subprocess
import sys
import time
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2]
PINS = ROOT / "ci/velopack-prototype.json"
APP_ID = "org.perikop.UpdateProbe"


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def prepare(output):
    """Network is permitted only in this explicit preparation step."""
    pins = json.loads(PINS.read_text())
    downloads = output / "downloads"
    downloads.mkdir(parents=True, exist_ok=True)
    for asset in pins["assets"]:
        archive = downloads / asset["file"]
        if not archive.is_file() or digest(archive) != asset["sha256"]:
            url = asset.get("url", f"https://github.com/velopack/velopack/releases/download/{pins['version']}/{asset['file']}")
            partial = archive.with_suffix(archive.suffix + ".partial")
            with urllib.request.urlopen(url, timeout=120) as source, partial.open("wb") as target:
                shutil.copyfileobj(source, target)
            if digest(partial) != asset["sha256"]:
                raise ValueError("SHA-256 mismatch: " + asset["file"])
            partial.replace(archive)
        if "destination" not in asset:
            print("Verified " + asset["file"], flush=True)
            continue
        destination = output / asset["destination"]
        if destination.exists():
            shutil.rmtree(destination)
        with zipfile.ZipFile(archive) as package:
            for name in package.namelist():
                if not (destination / name).resolve().is_relative_to(destination.resolve()):
                    raise ValueError("Unsafe archive path: " + name)
            package.extractall(destination)
        print("Verified and extracted " + asset["file"], flush=True)
    # zipfile does not restore executable permissions; vpk's Linux vendor tools need them.
    for path in (output / "vpk/vendor").iterdir():
        if path.is_file() and path.read_bytes()[:4] == b"\x7fELF":
            path.chmod(0o755)


def command(arguments, *, env=None, timeout=180, log=None, input_text=None):
    arguments = [str(value) for value in arguments]
    print("+ " + shlex.join(arguments), flush=True)
    result = subprocess.run(arguments, env=env, input=input_text, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=timeout)
    if log:
        log.write_text(result.stdout)
    if result.returncode:
        raise RuntimeError(f"Command exited {result.returncode}: {shlex.join(arguments)}\n{result.stdout}")
    return result.stdout


def run(output):
    if sys.platform != "linux":
        raise ValueError("Run this probe inside the Linux compatibility container")
    architectures = {"aarch64": "arm64", "arm64": "arm64", "x86_64": "x64"}
    architecture = architectures.get(platform.machine())
    if not architecture:
        raise ValueError("Unsupported Linux architecture: " + platform.machine())
    # Recheck the input archives even when preparation happened on another machine.
    for asset in json.loads(PINS.read_text())["assets"]:
        if digest(output / "downloads" / asset["file"]) != asset["sha256"]:
            raise ValueError("Prototype input changed: " + asset["file"])
    work = output / ("run-" + architecture)
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)
    environment = dict(os.environ, DOTNET_CLI_HOME=str(work / "dotnet-home"),
                       DOTNET_CLI_TELEMETRY_OPTOUT="1", DOTNET_SKIP_FIRST_TIME_EXPERIENCE="1",
                       XDG_CACHE_HOME=str(work / "cache"), XDG_DATA_HOME=str(work / "data"),
                       APPIMAGE_EXTRACT_AND_RUN="1")
    sdk = output / "sdk"
    packaging = output / "vpk/tools/net8.0/any/vpk.dll"
    tool = ["dotnet", packaging]
    baseline = command(["ldd", "--version"])
    print(baseline.splitlines()[0], flush=True)
    if "2.34" not in baseline.splitlines()[0]:
        raise ValueError("This compatibility probe must run against the glibc 2.34 baseline")

    # Preserve evidence of the upstream shared library's actual loader compatibility.
    dynamic = sdk / f"lib/velopack_libc_linux_{architecture}_gnu.so"
    load = subprocess.run([sys.executable, "-c", "import ctypes,sys; ctypes.CDLL(sys.argv[1])", str(dynamic)],
                          text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (work / "shared-sdk-load.txt").write_text(load.stdout)
    print("Upstream shared SDK load: " + ("passed" if load.returncode == 0 else "failed; see shared-sdk-load.txt"),
          flush=True)

    feeds = work / "feeds"
    feeds.mkdir()
    installed = work / "installed.AppImage"
    releases = f"linux-{architecture}-releases"
    nightly = f"linux-{architecture}-nightly"
    profile = work / "profile/state.json"
    profile.parent.mkdir()
    profile.write_text('{"theme":"dark","bookmark":"Ps 22:1","update_channel":"releases"}\n')
    original_profile = profile.read_bytes()

    def pack(version, channel):
        publish = work / "publish"
        publish.mkdir(exist_ok=True)
        executable = publish / "perikop-update-probe"
        command(["g++", "-std=c++17", "-O2", "-isystem", sdk / "include",
                 f'-DPROBE_VERSION="{version}"', ROOT / "tools/ci/velopack_probe.cpp",
                 sdk / f"lib-static/velopack_libc_linux_{architecture}_gnu.a",
                 "-pthread", "-ldl", "-lm", "-o", executable])
        command([sys.executable, ROOT / "tools/check_linux_abi.py", executable],
                log=work / ("abi-" + version + ".txt"))
        shutil.copyfile(ROOT / "LICENSE", publish / "Perikop-LICENSE.txt")
        shutil.copyfile(output / "downloads/velopack-LICENSE.txt", publish / "Velopack-LICENSE.txt")
        command([*tool, "pack", "--packId", APP_ID, "--packVersion", version,
                 "--packDir", publish, "--mainExe", executable.name, "--packTitle", "Perikop Update Probe",
                 "--runtime", "linux-" + architecture, "--channel", channel, "--delta", "None",
                 "--icon", ROOT / "resources/icons/OrthodoxReader.iconset/icon_256x256.png",
                 "--outputDir", feeds], env=environment, log=work / ("pack-" + version + ".log"))

    def app(*args):
        return command([installed, *args], env=environment, timeout=45)

    def expect_version(version):
        result = app("version")
        if "VERSION " + version + "\n" not in result:
            raise ValueError("Unexpected installed version: " + result)

    def trace_startup(name, version):
        trace = work / (name + ".trace")
        result = command(["strace", "-f", "-e", "trace=network", "-o", trace,
                          installed, "version"], env=environment, timeout=45)
        if "VERSION " + version + "\n" not in result:
            raise ValueError("Startup unexpectedly applied a staged update: " + result)
        if re.search(r"\bAF_INET6?\b", trace.read_text()):
            raise ValueError("Startup attempted Internet networking; see " + str(trace))

    def update(channel, version, downgrade=False):
        available = app("check", feeds, channel)
        expected = f"AVAILABLE {version} DOWNGRADE {int(downgrade)}"
        if expected not in available:
            raise ValueError("Unexpected update: " + available)
        app("stage", feeds, channel)
        before = app("version").split("VERSION ")[-1].strip()
        trace_startup("staged-" + before + "-to-" + version, before)
        # Download and apply in the same SDK lifetime, retaining the exact selected asset.
        # A prior startup may have cleaned a cached downgrade; only this explicit stage may fetch it again.
        session = command([installed, "session", feeds, channel], env=environment, timeout=90,
                          input_text="stage\nversion\napply\n", log=work / ("apply-" + version + ".log"))
        if "STAGED\n" not in session or "VERSION " + before + "\n" not in session or "APPLY_REQUESTED\n" not in session:
            raise ValueError("Unexpected download/apply session: " + session)
        deadline = time.monotonic() + 60
        while time.monotonic() < deadline:
            try:
                result = app("version")
                if "VERSION " + version + "\n" in result:
                    break
            except (OSError, RuntimeError):
                pass  # Replacement can briefly make the previous inode unavailable.
            time.sleep(1)
        else:
            raise ValueError("Updater did not replace the installed AppImage")
        if profile.read_bytes() != original_profile:
            raise ValueError("Updating modified the external profile fixture")
        print("Applied " + version + " via " + channel, flush=True)

    pack("1.0.0", releases)
    images = list(feeds.glob("*.AppImage"))
    if len(images) != 1:
        raise ValueError("Expected one initial AppImage: " + str(images))
    shutil.copyfile(images[0], installed)
    installed.chmod(0o755)
    expect_version("1.0.0")
    trace_startup("initial-startup", "1.0.0")
    pack("1.0.1", releases)
    pack("1.0.2-nightly.1", nightly)
    update(releases, "1.0.1")
    update(nightly, "1.0.2-nightly.1")
    update(releases, "1.0.1", downgrade=True)
    if "NO_UPDATE" not in app("check", feeds, releases):
        raise ValueError("Already current release was offered again")
    summary = {"velopack": json.loads(PINS.read_text())["version"], "architecture": architecture,
               "glibc": "2.34", "sdk_linkage": "static", "shared_sdk_loads": load.returncode == 0,
               "upgrade": "passed", "nightly_switch": "passed", "return_to_releases": "passed",
               "startup_networking": "none", "auto_apply_on_startup": False,
               "external_profile": "preserved"}
    (work / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("prepare", "run"))
    parser.add_argument("--output", type=Path, default=ROOT / "build/velopack-prototype")
    args = parser.parse_args()
    output = args.output.resolve()
    try:
        (prepare if args.action == "prepare" else run)(output)
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
        parser.exit(1, str(error) + "\n")


if __name__ == "__main__":
    main()
