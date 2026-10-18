# AppImage runtime and libfuse (LGPL-2.1)

October 18, 2026. This is an engineering record, not legal advice.

## What ships

Every Linux AppImage starts with the AppImage type 2 runtime, a static program (static-pie, musl) that mounts or extracts the SquashFS file system after it. Perikop ships two runtime builds, identified by SHA-256 and by the commit each prints with `--appimage-version`:

| Packaging | type2-runtime commit | x86_64 SHA-256 | aarch64 SHA-256 |
| --- | --- | --- | --- |
| linuxdeploy (`Perikop-<version>-<arch>.AppImage`) | `8f39b89e2ac31e1640b3d3f7e9a5108e6ce805fa` | `156f4bdbde9c52d01814600013e0a273f0118dc2de98975f3c8c63427ec79074` | `b4ff0030242d0c3bb12ce40541828303cf167493f4793456f0436edd6255c39d` |
| Velopack (vendored in vpk 1.2.161) | `3d17002ee2a519a288e28aeb7bc41af5f5adaf4e` | `a2419dce47568395ae79c01ffa9a5a341dd339581352ff104d073527543177e5` | `7f27a8c15bf20a2e46342ea4a977047a69d8b4d64a123fe0b5e23b20fd290c85` |

The linuxdeploy runtime is pinned in `ci/upstreams.json`; the Velopack runtime comes from the pinned `vpk.nupkg`. `tools/appimage_runtime.py` holds the full hashes. The two commits differ only in `src/runtime/runtime.c` and upstream CI; their Dockerfile, dependency script, Makefile, and libfuse patch are identical.

Both runtimes statically link:

| Component | Version | License | Origin |
| --- | --- | --- | --- |
| libfuse (library) | 3.15.0, patched | LGPL-2.1 | `fuse-3.15.0.tar.xz` + `patches/libfuse/mount.c.diff` |
| squashfuse | 0.5.2 | BSD-2-Clause | `0.5.2.tar.gz` |
| zstd | 1.5.6 | BSD-3-Clause (dual with GPL-2.0; used under BSD) | Alpine 3.21 `zstd-static` |
| zlib | 1.3.2 (linuxdeploy), 1.3.1 (Velopack) | zlib | Alpine 3.21 `zlib-static` |
| mimalloc | 2.1.7 | MIT | Alpine 3.21 `mimalloc2-dev` |
| musl libc | 1.2.5 | MIT | Alpine 3.21 `musl-dev` |

Versions come from strings in the binaries (libfuse, zstd, zlib) and from Alpine 3.21's package index (musl 1.2.5-r11, mimalloc2 2.1.7-r0). libfuse's `LICENSE` puts only `include/`, `lib/`, and `meson.build` under the LGPL; its utilities are GPL-2.0, but the runtime links only `libfuse3` and calls the host's `fusermount3` at run time. No other copyleft code is linked into the runtime.

## What LGPL-2.1 asks here

The runtime is a "work that uses the Library", linked statically, so Section 6 applies. Perikop distributes the runtime as part of each AppImage, so these apply to Perikop's distribution:

- Required: give prominent notice that libfuse is used and is under the LGPL, and include the license text (Section 6 first paragraph). Done: `THIRD-PARTY-NOTICES.md` and `resources/licenses/libfuse-LGPL-2.1.txt`, inside every AppImage.
- Required: one of Section 6 (a)–(e). Perikop uses 6(a) through 6(d): every release and nightly that carries AppImages also carries `AppImage-runtime-source-8f39b89-3d17002.tar.gz` in the same place. It holds the complete source of libfuse 3.15.0 with upstream's patch, and of the work that uses it: both runtime revisions with their build scripts, plus squashfuse, zstd, zlib, mimalloc, and musl. With these, a user can rebuild the runtime and relink it with a modified libfuse.
- Required: the modification to libfuse (the `mount.c` patch) is included as source, in each runtime archive.
- Recommended, not strictly required: version-pinned sources of the permissive libraries. Since they are linked into the same executable, the Section 6 exception for operating-system components does not clearly apply, so the archive includes them instead of relying on Alpine.

For a static executable, Section 6(a) requires "the complete machine-readable 'work that uses the Library', as object code and/or source code". Complete, buildable source of the runtime meets this, because a user can recompile it as well as relink it. Prebuilt object files are not also needed.

## Verified

- The pinned runtimes match the hashes above and name commit 8f39b89; vpk's runtimes name 3d17002.
- libfuse and squashfuse match the SHA-256 values in upstream's `install-dependencies.sh`. The six license texts are byte-identical to the upstream files at the pinned tags.
- In an `alpine:3.21` container on aarch64, the runtime was rebuilt from the archive alone, as its README describes. libfuse and squashfuse came from the archive, and the build used upstream's `scripts/build-runtime.sh`. The result is a static-pie runtime of the same size as the pinned build (936,456 bytes). It was not byte-identical; upstream's build is not reproducible.
- A visible change to `lib/mount.c` appeared in the relinked runtime. Following the README, that runtime was put in front of a Perikop AppImage's file system on Rocky Linux 9 (glibc 2.34). The result printed commit 8f39b89, extracted, and started Perikop.
- `tools/appimage_runtime.py check` passed on Rocky Linux 9 aarch64. Both the linuxdeploy AppImage and a freshly packed Velopack AppImage start with their pinned runtimes and carry every runtime license and `THIRD-PARTY-NOTICES.md`.

## Remaining risks

- Alpine package revisions are not pinned upstream. The archive holds the upstream releases of musl, zlib, zstd, and mimalloc, not Alpine's patched packages (such as musl 1.2.5-r11). A relinked runtime is functionally equivalent, but not bit-for-bit what upstream built. The Alpine revisions used for the Velopack runtime (built in March 2026) cannot be identified from the binary, except zlib 1.3.1.
- The x86_64 rebuild and relink were not run locally; only aarch64 was. Both architectures share one source and build path.
- The FUSE mount path (as opposed to extraction) was not exercised with a relinked runtime, because the test containers have no FUSE.
- AppImages published before this change carried the same runtime without the libfuse license or a source archive. This change does not fix those past releases.
- Whether this fully satisfies LGPL-2.1 is a legal judgment this record does not make.

## Reproduce

```sh
python3 tests/appimage_licenses_test.py
python3 tools/ci/dependencies.py restore --platform Linux
python3 tools/appimage_runtime.py source
tools/package_appimage.sh build/cmake && python3 tools/appimage_runtime.py check build/cmake   # on Linux
```
