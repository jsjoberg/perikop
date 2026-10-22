#!/usr/bin/env python3
"""The AppImage runtime in Perikop's AppImages: its corresponding source, and checks of what ships.

Every AppImage starts with a small program, the AppImage type 2 runtime, which mounts the rest. The runtime is
MIT-licensed but statically links libfuse 3.15.0, which is LGPL-2.1, so each release also carries the source to
rebuild the runtime, or relink it with a modified libfuse.

    python3 tools/appimage_runtime.py source [--archives build/ci/archives] [--output build/publish/updates]
        Writes AppImage-runtime-source-<commit>.tar.gz from the pinned inputs that dependencies.py restored.
    python3 tools/appimage_runtime.py check BUILD [--vpk build/ci/tools/vpk]
        On Linux after tools/prepare_appdir.sh: checks the runtime and that Velopack's AppImage carries the licences.
"""
import argparse
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
UPSTREAMS = ROOT / "ci/upstreams.json"
# The runtime Perikop distributes: the one vpk vendors and puts in front of every Velopack AppImage.
RUNTIME = {"commit": "3d17002ee2a519a288e28aeb7bc41af5f5adaf4e",
           "x86_64": "a2419dce47568395ae79c01ffa9a5a341dd339581352ff104d073527543177e5",
           "aarch64": "7f27a8c15bf20a2e46342ea4a977047a69d8b4d64a123fe0b5e23b20fd290c85"}
# Licences of everything linked into the runtime; each also ships inside the AppImages.
LICENCES = ("AppImage-type2-runtime-MIT.txt", "libfuse-LGPL-2.1.txt", "libfuse-LICENSE.txt", "squashfuse-LICENSE.txt",
            "zstd-LICENSE.txt", "zlib-LICENSE.txt", "musl-COPYRIGHT.txt", "mimalloc-LICENSE.txt")
# A fixed time for archive members, so the archive's bytes depend only on its inputs.
EPOCH = 946684800


def digest(path):
    with open(path, "rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def inputs():
    """The pinned source archives from ci/upstreams.json."""
    return [item for item in json.loads(UPSTREAMS.read_text()).values() if item.get("appimage_runtime_source")]


def name():
    return "AppImage-runtime-source-" + RUNTIME["commit"][:7]


def readme(items):
    sources = "\n".join(f"- `sources/{item['file']}`: {item['url']}\n  SHA-256 `{item['sha256']}`" for item in items)
    return f"""# Corresponding source of the AppImage runtime in Perikop

Perikop's Linux AppImages start with the AppImage type 2 runtime, which mounts the rest of the file.
The runtime is MIT-licensed. It statically links libfuse 3.15.0, whose library is licensed under the
GNU Lesser General Public License, version 2.1 (`licenses/libfuse-LGPL-2.1.txt`), and these
permissively licensed libraries: squashfuse 0.5.2, zstd 1.5.6, zlib 1.3.1, mimalloc 2.1.7, and musl 1.2.5.
Perikop itself is MIT-licensed; only the runtime contains LGPL code.

You may modify libfuse and relink the runtime with it, and use the result with Perikop's AppImages.
This archive holds what you need: the runtime's source and build scripts, the source of every
library it links, and upstream's libfuse patch.

## Which runtime

Perikop's AppImages and their updates use the runtime vendored in Velopack's vpk 1.2.161, type2-runtime
commit `{RUNTIME['commit']}`:

| Architecture | Runtime SHA-256 |
| --- | --- |
| x86_64 | `{RUNTIME['x86_64']}` |
| aarch64 | `{RUNTIME['aarch64']}` |

The runtime names its commit: `strings Perikop-*.AppImage | grep type2-runtime/commit`.

## Sources

{sources}

`SHA256SUMS` lists every file in this archive. The runtime archive contains `patches/libfuse/mount.c.diff`,
upstream's only change to libfuse: it reads the path of `fusermount3` from `$FUSERMOUNT_PROG`.

## Rebuild a runtime

Upstream builds the runtime in Alpine Linux 3.21 (`scripts/docker/Dockerfile`), with musl, zlib, zstd, and
mimalloc from Alpine's static packages. When this runtime was built, Alpine 3.21 had musl 1.2.5,
zstd 1.5.6, mimalloc 2.1.7, and zlib 1.3.1; this archive holds those upstream releases.

1. Unpack the runtime archive from `sources/` and write its commit URL to `src/runtime/version`:
   `https://github.com/AppImage/type2-runtime/commit/{RUNTIME['commit'][:7]}`.
2. Build in Alpine 3.21 as `BUILD.md` describes: `env ARCH=x86_64 scripts/docker/build-with-docker.sh`, or `aarch64`.
   `scripts/common/install-dependencies.sh` downloads `fuse-3.15.0.tar.xz` and `0.5.2.tar.gz` (squashfuse) and
   checks the same SHA-256 values as here; copy them from `sources/` instead to build without a network.

## Relink with a modified libfuse

1. Unpack `sources/fuse-3.15.0.tar.xz`, apply `patches/libfuse/mount.c.diff` with `patch -p1`, and make your changes.
2. Build and install it as a static library, as `scripts/common/install-dependencies.sh` does:
   `meson setup --prefix=/usr build && meson configure build --default-library static && ninja -C build install`.
3. Build squashfuse and the runtime as in that script and `scripts/build-runtime.sh`; the runtime's
   `src/runtime/Makefile` links `-lfuse3`, so it now contains your libfuse.
4. Put your runtime in front of a Perikop AppImage's file system:

   ```sh
   offset=$(./Perikop.AppImage --appimage-offset)
   tail -c +$((offset + 1)) Perikop.AppImage > filesystem.squashfs
   cat runtime-x86_64 filesystem.squashfs > Perikop-relinked.AppImage
   chmod +x Perikop-relinked.AppImage
   ```

The licences of the runtime and of every library it links are in `licenses/`, and in each AppImage under
`usr/share/perikop/resources/licenses/`.
"""


def member(archive, path, data):
    info = tarfile.TarInfo(path)
    info.size, info.mtime, info.mode = len(data), EPOCH, 0o644
    info.uid = info.gid = 0
    info.uname = info.gname = ""
    archive.addfile(info, io.BytesIO(data))


def source(archives, output):
    """Write the archive; the same inputs always give the same bytes."""
    items = inputs()
    files = {}
    for item in items:
        path = archives / item["file"]
        if not path.is_file() or digest(path) != item["sha256"]:
            raise SystemExit(f"Missing or changed runtime source: {path}. Restore the pinned Linux inputs first.")
        files["sources/" + item["file"]] = path.read_bytes()
    for licence in LICENCES:
        files["licenses/" + licence] = (ROOT / "resources/licenses" / licence).read_bytes()
    files["README.md"] = readme(items).encode()
    files["SHA256SUMS"] = "".join(f"{hashlib.sha256(data).hexdigest()}  {path}\n"
                                  for path, data in sorted(files.items())).encode()
    output.mkdir(parents=True, exist_ok=True)
    target = output / (name() + ".tar.gz")
    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode="w", format=tarfile.PAX_FORMAT) as archive:
        for path, data in sorted(files.items()):
            member(archive, name() + "/" + path, data)
    with open(target, "wb") as raw, gzip.GzipFile(fileobj=raw, mode="wb", mtime=0, filename="") as compressed:
        compressed.write(buffer.getvalue())
    return target


def runtime_problem(data, architecture):
    """Why this runtime is not the pinned one, or None."""
    if hashlib.sha256(data).hexdigest() != RUNTIME[architecture]:
        return f"the runtime for {architecture} is not the pinned build"
    return appimage_problem(data)


def appimage_problem(data):
    """Why this AppImage (or runtime) does not start with the pinned runtime, or None."""
    commit = RUNTIME["commit"][:7]
    if f"github.com/AppImage/type2-runtime/commit/{commit}".encode() not in data[:4 << 20]:
        return f"the runtime does not name commit {commit}; update RUNTIME and the corresponding source"
    return None


def licences_problem(appimage, work):
    """Why the AppImage's file system lacks the runtime licences or the notices, or None."""
    subprocess.run([str(appimage), "--appimage-extract"], cwd=work, check=True, capture_output=True,
                   env=dict(os.environ, APPIMAGE_EXTRACT_AND_RUN="1"))
    resources = work / "squashfs-root/usr/share/perikop/resources"
    missing = [licence for licence in LICENCES
               if not (resources / "licenses" / licence).is_file() or
               (resources / "licenses" / licence).read_bytes() != (ROOT / "resources/licenses" / licence).read_bytes()]
    if not (resources / "THIRD-PARTY-NOTICES.md").is_file():
        missing.append("THIRD-PARTY-NOTICES.md")
    return f"{appimage.name} lacks {', '.join(missing)}" if missing else None


def check(build, vpk):
    architecture = platform.machine()
    if architecture not in ("x86_64", "aarch64"):
        raise SystemExit("AppImages exist only for x86_64 and aarch64")
    problems = [runtime_problem((vpk / f"vendor/appimagekit-runtime-{architecture}").read_bytes(), architecture)]
    with tempfile.TemporaryDirectory() as temporary:
        work = Path(temporary)
        # Velopack makes the AppImage from the prepared AppDir; pack one to check what it ships.
        channel = "linux-" + {"x86_64": "x64", "aarch64": "arm64"}[architecture] + "-nightly"
        subprocess.run([str(ROOT / "tools/package_velopack.sh"), str(build), "1.0.0-licences", channel, str(work / "feed")],
                       check=True, capture_output=True, env=dict(os.environ, PERIKOP_VPK=str(vpk)))
        image = next((work / "feed").glob("*.AppImage"))
        problems.append(appimage_problem(image.read_bytes()))
        (work / "extract").mkdir()
        problems.append(licences_problem(image, work / "extract"))
    problems = [problem for problem in problems if problem]
    if problems:
        raise SystemExit("AppImage runtime check failed:\n- " + "\n- ".join(problems))
    print(f"{architecture}: the AppImage starts with the pinned runtime and carries its licences")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    making = commands.add_parser("source")
    making.add_argument("--archives", type=Path, default=ROOT / "build/ci/archives")
    making.add_argument("--output", type=Path, default=ROOT / "build/publish/updates")
    checking = commands.add_parser("check")
    checking.add_argument("build", type=Path)
    checking.add_argument("--vpk", type=Path, default=ROOT / "build/ci/tools/vpk")
    args = parser.parse_args()
    if args.command == "source":
        print(source(args.archives, args.output))
    else:
        check(args.build.resolve(), args.vpk.resolve())


if __name__ == "__main__":
    main()
