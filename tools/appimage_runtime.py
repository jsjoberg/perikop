#!/usr/bin/env python3
"""The AppImage runtimes in Perikop's AppImages: their corresponding source, and checks of what ships.

Every AppImage starts with a small program, the AppImage type 2 runtime, which mounts the rest. The runtime is
MIT-licensed but statically links libfuse 3.15.0, which is LGPL-2.1, so each release also carries the source to
rebuild the runtime, or relink it with a modified libfuse.

    python3 tools/appimage_runtime.py source [--archives build/ci/archives] [--output build/publish/legacy]
        Writes AppImage-runtime-source-<commits>.tar.gz from the pinned inputs that dependencies.py restored.
    python3 tools/appimage_runtime.py check BUILD [--vpk build/ci/tools/vpk]
        On Linux after packaging: checks the runtimes and that the AppImages in build/package carry the licences.
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
# The runtimes Perikop distributes. linuxdeploy embeds the pinned runtime; vpk embeds its own vendored one.
RUNTIMES = {
    "linuxdeploy": {"commit": "8f39b89e2ac31e1640b3d3f7e9a5108e6ce805fa",
                    "x86_64": "156f4bdbde9c52d01814600013e0a273f0118dc2de98975f3c8c63427ec79074",
                    "aarch64": "b4ff0030242d0c3bb12ce40541828303cf167493f4793456f0436edd6255c39d"},
    "velopack": {"commit": "3d17002ee2a519a288e28aeb7bc41af5f5adaf4e",
                 "x86_64": "a2419dce47568395ae79c01ffa9a5a341dd339581352ff104d073527543177e5",
                 "aarch64": "7f27a8c15bf20a2e46342ea4a977047a69d8b4d64a123fe0b5e23b20fd290c85"},
}
# Licences of everything linked into the runtimes; each also ships inside the AppImages.
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
    return "AppImage-runtime-source-" + "-".join(runtime["commit"][:7] for runtime in RUNTIMES.values())


def readme(items):
    linuxdeploy, velopack = RUNTIMES["linuxdeploy"], RUNTIMES["velopack"]
    sources = "\n".join(f"- `sources/{item['file']}`: {item['url']}\n  SHA-256 `{item['sha256']}`" for item in items)
    return f"""# Corresponding source of the AppImage runtimes in Perikop

Perikop's Linux AppImages start with the AppImage type 2 runtime, which mounts the rest of the file.
The runtime is MIT-licensed. It statically links libfuse 3.15.0, whose library is licensed under the
GNU Lesser General Public License, version 2.1 (`licenses/libfuse-LGPL-2.1.txt`), and these
permissively licensed libraries: squashfuse 0.5.2, zstd 1.5.6, zlib, mimalloc 2.1.7, and musl 1.2.5.
Perikop itself is MIT-licensed; only the runtime contains LGPL code.

You may modify libfuse and relink the runtime with it, and use the result with Perikop's AppImages.
This archive holds what you need: the runtime's source and build scripts, the source of every
library it links, and upstream's libfuse patch.

## Which runtimes

| AppImage | Runtime commit | x86_64 runtime SHA-256 | aarch64 runtime SHA-256 |
| --- | --- | --- | --- |
| `Perikop-<version>-<arch>.AppImage` (linuxdeploy) | `{linuxdeploy['commit']}` | `{linuxdeploy['x86_64']}` | `{linuxdeploy['aarch64']}` |
| Velopack AppImages and updates | `{velopack['commit']}` | `{velopack['x86_64']}` | `{velopack['aarch64']}` |

Each runtime names its commit: `strings Perikop-*.AppImage | grep type2-runtime/commit`.
The two commits differ only in `src/runtime/runtime.c`; their dependencies, build scripts, and libfuse patch are identical.

## Sources

{sources}

`SHA256SUMS` lists every file in this archive. Each runtime archive contains `patches/libfuse/mount.c.diff`,
upstream's only change to libfuse: it reads the path of `fusermount3` from `$FUSERMOUNT_PROG`.

## Rebuild a runtime

Upstream builds the runtime in Alpine Linux 3.21 (`scripts/docker/Dockerfile`), with musl, zlib, zstd, and
mimalloc from Alpine's static packages. When these runtimes were built, Alpine 3.21 had musl 1.2.5,
zstd 1.5.6, mimalloc 2.1.7, and zlib 1.3.2 (zlib 1.3.1 for the Velopack runtime); this archive holds
those upstream releases.

1. Unpack a runtime archive from `sources/` and write its commit URL to `src/runtime/version`, such as
   `https://github.com/AppImage/type2-runtime/commit/{linuxdeploy['commit'][:7]}`.
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


def runtime_problem(data, packaging, architecture):
    """Why this runtime is not the pinned one, or None."""
    runtime = RUNTIMES[packaging]
    if hashlib.sha256(data).hexdigest() != runtime[architecture]:
        return f"the {packaging} runtime for {architecture} is not the pinned build"
    return appimage_problem(data, packaging)


def appimage_problem(data, packaging):
    """Why this AppImage (or runtime) does not start with the pinned runtime, or None."""
    commit = RUNTIMES[packaging]["commit"][:7]
    if f"github.com/AppImage/type2-runtime/commit/{commit}".encode() not in data[:4 << 20]:
        return f"the runtime does not name commit {commit}; update RUNTIMES and the corresponding source"
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
    tools = build.parent / "ci/tools"
    problems = [runtime_problem((tools / f"appimage/runtime-{architecture}").read_bytes(), "linuxdeploy", architecture),
                runtime_problem((vpk / f"vendor/appimagekit-runtime-{architecture}").read_bytes(), "velopack", architecture)]
    images = sorted((ROOT / "build/package").glob(f"Perikop-*-{architecture}.AppImage"))
    if len(images) != 1:
        raise SystemExit(f"Expected one {architecture} AppImage in build/package, found {images}")
    with tempfile.TemporaryDirectory() as temporary:
        work = Path(temporary)
        problems.append(appimage_problem(images[0].read_bytes(), "linuxdeploy"))
        (work / "linuxdeploy").mkdir()
        problems.append(licences_problem(images[0], work / "linuxdeploy"))
        # Velopack makes its AppImage from the same AppDir; pack one to check what it ships.
        channel = "linux-" + {"x86_64": "x64", "aarch64": "arm64"}[architecture] + "-nightly"
        subprocess.run([str(ROOT / "tools/package_velopack.sh"), str(build), "1.0.0-licences", channel, str(work / "feed")],
                       check=True, capture_output=True, env=dict(os.environ, PERIKOP_VPK=str(vpk)))
        velopack = next((work / "feed").glob("*.AppImage"))
        problems.append(appimage_problem(velopack.read_bytes(), "velopack"))
        (work / "velopack").mkdir()
        problems.append(licences_problem(velopack, work / "velopack"))
    problems = [problem for problem in problems if problem]
    if problems:
        raise SystemExit("AppImage runtime check failed:\n- " + "\n- ".join(problems))
    print(f"{architecture}: both AppImages start with the pinned runtimes and carry their licences")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    making = commands.add_parser("source")
    making.add_argument("--archives", type=Path, default=ROOT / "build/ci/archives")
    making.add_argument("--output", type=Path, default=ROOT / "build/publish/legacy")
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
