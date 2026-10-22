#!/usr/bin/env python3
"""Capture and relocate clang-tidy and ccache, including all non-system dylibs."""

from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile


def command(*args):
    return subprocess.check_output(args, text=True).strip()


def dylibs(path):
    return re.findall(r"^\s+(.+?) \(compatibility version", command("otool", "-L", str(path)), re.M)


def rpaths(path):
    output = command("otool", "-l", str(path))
    return re.findall(r"cmd LC_RPATH\n.*?\n\s+path (.+?) \(offset", output)


def resolve(name, loader):
    def expand(value):
        return Path(value.replace("@loader_path", str(loader.parent)).replace("@executable_path", str(loader.parent)))
    if name.startswith("@rpath/"):
        for directory in rpaths(loader):
            candidate = expand(directory) / name[len("@rpath/"):]
            if candidate.exists():
                return candidate.resolve()
        raise ValueError("Unresolved library {} in {}".format(name, loader))
    return expand(name).resolve(strict=True)


def main():
    output = Path(sys.argv[1]).resolve()
    staging = output / "macOS-tools"
    (staging / "bin").mkdir(parents=True, exist_ok=True)
    (staging / "lib").mkdir(parents=True, exist_ok=True)
    llvm = Path(command("brew", "--prefix", "llvm@23"))
    cache = Path(command("brew", "--prefix", "ccache"))
    mapping = {}
    pending = [(llvm / "bin" / name).resolve() for name in ("clang-tidy", "clang-apply-replacements")]
    pending.append((cache / "bin/ccache").resolve())
    for path in pending:
        mapping[path] = staging / "bin" / path.name
    links = {}
    while pending:
        path = pending.pop()
        target = mapping[path]
        shutil.copy2(path, target)
        links[path] = []
        for name in dylibs(path):
            if name.startswith(("/usr/lib/", "/System/Library/")):
                continue
            library = resolve(name, path)
            if library == path:  # dylib install name
                continue
            if library not in mapping:
                destination = staging / "lib" / library.name
                if destination in mapping.values():
                    raise ValueError("Duplicate library basename: " + library.name)
                mapping[library] = destination
                pending.append(library)
            links[path].append((name, library))
    for path, target in mapping.items():
        if target.parent.name == "lib":
            subprocess.run(["install_name_tool", "-id", "@rpath/" + target.name, str(target)], check=True)
        for old, library in links[path]:
            prefix = "@loader_path/../lib/" if target.parent.name == "bin" else "@loader_path/"
            subprocess.run(["install_name_tool", "-change", old, prefix + mapping[library].name, str(target)], check=True)
        subprocess.run(["codesign", "--force", "--sign", "-", str(target)], check=True)
    shutil.copy2(llvm / "bin/run-clang-tidy", staging / "bin/run-clang-tidy")
    # clang-tidy finds builtin headers relative to its own executable.
    for version in (llvm / "lib/clang").iterdir():
        if (version / "include").is_dir():
            shutil.copytree(version / "include", staging / "lib/clang" / version.name / "include")
    (staging / "LICENSE-LLVM.txt").write_text((llvm / "share/doc/llvm/LICENSE.txt").read_text()
                                             if (llvm / "share/doc/llvm/LICENSE.txt").exists()
                                             else "LLVM: Apache-2.0 WITH LLVM-exception. https://llvm.org/LICENSE.txt\n")
    (staging / "LICENSE-ccache.txt").write_text("ccache: GPL-3.0-or-later. https://ccache.dev/license.html\n")
    subprocess.run([str(staging / "bin/clang-tidy"), "--version"], check=True)
    subprocess.run([str(staging / "bin/ccache"), "--version"], check=True)
    with tempfile.TemporaryDirectory() as temporary:
        probe = Path(temporary) / "resource-probe.cpp"
        probe.write_text("#include <stddef.h>\nsize_t size_of(void* p) { return sizeof(p); }\n")
        subprocess.run([str(staging / "bin/clang-tidy"), "--checks=clang-analyzer-core*", str(probe),
                        "--", "-std=c++23", "-isysroot", command("xcrun", "--show-sdk-path")], check=True)
    with tarfile.open(output / "macOS-tools.tar.gz", "w:gz", compresslevel=1) as archive:
        for entry in staging.iterdir():
            archive.add(entry, arcname=entry.name)


if __name__ == "__main__":
    main()
