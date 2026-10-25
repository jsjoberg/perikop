# Linux packaging plan: Debian 13, EL 10 and a Debian-built AppImage

Status, 2026-10-25: steps 1 and 2 are done (#52, #54), and the AppImage is held to glibc 2.39 so it runs on Enterprise Linux 10 (#61). Steps 3 to 5 (`.deb`, `.rpm` and their release plumbing) are dropped: the AppImage covers Debian 13, Enterprise Linux 10 and the newer distributions on its own. On AlmaLinux 10 it passes the smoke test apart from clipboard and window-placement checks that fail the same way for Rocky-built AppImages in the headless Mutter session. Step 2 lists where the result differs from the plan.

## Decisions

- Linux support follows what Debian 13 and EL 10 (AlmaLinux/Rocky 10) ship. Their stock compilers are the constraint: **GCC 14** becomes the minimum (Debian 13: 14.2, EL 10: 14.3). No gcc-toolset and no custom compilers.
- **Debian 13** builds the `.deb` and the **AppImage**, for x64 and arm64 (Raspberry Pi OS is Debian 13 based).
- **AlmaLinux 10** builds the `.rpm`.
- The AppImage keeps its Velopack updates. `.deb` and `.rpm` copies are updated by their package manager or by a new download. There are no APT or DNF repositories in this plan.
- Windows and macOS are unchanged.

## Why this works

Checked locally on arm64, 2026-10-23/24, with main at `c75815c`:

| Check | Result |
|---|---|
| Debian 13, stock `g++` 14.2, system GTK and fontconfig | Builds with no warnings in our code. All 18 CTests pass under Xvfb, with `PERIKOP_REQUIRE_ESPEAK=1` and Debian's eSpeak NG. |
| AlmaLinux 10, gcc-toolset-15 (stock GCC 14.3 not tried yet; step 4 must check it) | Builds. The app needs glibc 2.39 (`pidfd_spawnp`), the bundled GTK stack ≤ 2.38. GUI tests fail because EL 10 has no Xvfb (see "Open decisions"). |
| GCC 15 requirement | Added in #6 as a toolchain choice; no code needs it. |

## Support matrix after the change

| Package | Built on | glibc needed | Runs on |
|---|---|---|---|
| AppImage x64/arm64 | Debian 13 | 2.39 (gate since `linux-glibc239`) | Enterprise Linux 10, Debian 13, Raspberry Pi OS (Debian 13), Ubuntu 26.04, Fedora 40+, Arch, Slackware-current |
| `.deb` x64/arm64 | Debian 13 | Debian 13 | Debian 13, Raspberry Pi OS (Debian 13) |
| `.rpm` x64 (arm64 optional) | AlmaLinux 10 | 2.39 | AlmaLinux, Rocky and RHEL 10 |

No longer supported: Ubuntu 24.04 and older, Debian 12, EL 9, Slackware 15.0. Today's AppImage (glibc 2.34) keeps working on several of these until the first release built by the new system.

Security support: Debian 13 until June 2030 (paid extended support to 2035), EL 10 until May 2035.

## Work, as separate pull requests

Each PR keeps CI green on its own.

### 1. GCC 14 minimum (done, #52)

- `CMakeLists.txt`: lower the GCC check from 15 to 14 for Linux; keep 15 for Windows (w64devkit) unless it also builds with 14.
- `docs/building.md`: requirements for Debian 13 and EL 10 with their own compilers.
- CI stays on the Rocky 9 image for now; its GCC 15 still passes the check.

### 2. Debian 13 replaces Rocky 9 for the Linux CI image (done, #54)

Differences from the plan:
- .NET 8 comes from Microsoft's runtime tarball, pinned by SHA-512, because Microsoft's Debian 13 repository has no arm64 build.
- Every bundled package has a Debian copyright file, so `tools/appimage-licenses/` was removed. Packaging stops when a bundled library has no package owner.
- `ci/lsan.supp` also suppresses Mesa's GLX state (`libGLX_mesa.so.0`).
- Linux update channels now carry `-glibc241` after the architecture (renamed `-glibc239` when the gate moved to Enterprise Linux 10's 2.39), so clients built for glibc 2.34 never receive a Debian 13 build. See `docs/updating.md`.
- The Ubuntu 26.04 smoke failure was older than this change. It was fixed in #55 and is tested under a subpixel fontconfig host since #56.

- `ci/linux.Dockerfile`: `FROM debian:trixie`. Install the compiler, CMake, Ninja, ccache, GTK and fontconfig headers, Xvfb, the sanitizer runtimes, `clang-tidy-22`, squashfs-tools and dpkg-dev. Velopack's `vpk` needs the .NET 8 runtime, which Debian does not package; take it from Microsoft's Debian 13 repository (packages.microsoft.com). The snapshot captures it like the rest of the image. Check whether the image excludes `/usr/share/doc`; the licence collection needs the `copyright` files.
- `tools/ci/build.py` and `build.yml`: drop `/opt/rh/gcc-toolset-15` from `PATH`; the tidy target uses Debian's clang-tidy 22.
- `tools/check_linux_abi.py` and its test: limit 2.34 → 2.41; remove the `_dl_find_object` backport exception.
- `tools/prepare_appdir.sh`: collect licences with `dpkg -S` and `/usr/share/doc/<package>/copyright` instead of `rpm`. Then check whether `tools/appimage-licenses/` is still needed.
- `ci/lsan.supp`: re-run `linux-sanitizers` and confirm the fontconfig and librsvg rules still match on Debian's libraries.
- `THIRD-PARTY-NOTICES.md`: point the bundled system libraries at Debian's source packages instead of Rocky's.
- `tools/ci/velopack_linux_test.py`: describe the new baseline.
- Docs: `using-perikop.md` (new requirements), `building.md`, `releasing.md`, `updating.md`, `testing.md`.
- Dependency snapshot: rebuild and pin it (ask before triggering `dependencies.yml`).
- Verification: every CI job green; the AppImage passes `--smoke-test` on clean Debian 13, Ubuntu 26.04, Fedora 43, Arch and Raspberry Pi OS (Debian 13, arm64) containers.

### 3. `.deb` from the Debian 13 job (dropped)

- Build with CPack DEB (or `dpkg-deb`), computing dependencies from the binaries with `dpkg-shlibdeps`. The package carries a desktop entry and an icon. wxWidgets stays statically linked from the snapshot.
- Do not install files that the package manager and Velopack would both claim.
- Verification: install on clean Debian 13 and Raspberry Pi OS containers, run `--smoke-test`, uninstall cleanly.

### 4. `.rpm` from an AlmaLinux 10 job (dropped)

- A new environment, `ci/el10.Dockerfile`, on `almalinux/10-base`. Use the `linux/amd64/v2` variant so older CPUs (X220 era) keep working. It needs its own snapshot environment.
- Build with CPack RPM, with automatic library dependencies.
- GUI tests: see "Open decisions".
- Verification: install on clean AlmaLinux 10 and Rocky 10 containers, run `--smoke-test`, uninstall cleanly.

### 5. Release plumbing (dropped)

- `tools/ci/update_packages.py`: allow `.deb` and `.rpm` assets in `assemble`. They have no Velopack feed.
- `tools/ci/nightly.py` and the release notes: list the new packages.
- `src/app/updates.cpp`: a copy that Velopack did not install already reports "not installed". Make the message point package installs to their package manager or the Releases page.

## Open decisions (moot since steps 3 to 5 were dropped)

1. **GUI tests on EL 10.** EL 10 has no Xvfb. Headless Weston has no input seat, and Perikop crashes there (separate bug, see below). Under Mutter or cage, the clipboard checks (`main_frame_smoke.cpp` lines 367 and 1507) and the window placement checks (lines 406, 428 and 445) fail, because the compositor manages window geometry. Options:
   - Build Xvfb for EL 10 from Fedora's `xorg-x11-server` source package in the snapshot workflow.
   - Make those checks valid under a compositor that manages windows. Users on Wayland desktops also run that way.
2. **Nightly `.deb`/`.rpm`.** Publish them with every nightly, or only with releases.
3. **Package names and install location**, for example `perikop` in `/usr` or `/opt/perikop`.
4. **Ubuntu 24.04.** Decided: dropped, with Debian 12 and Enterprise Linux 9 (#54).

## Found along the way

- Perikop segfaults when the display has no input seat (headless Weston), after GTK reports `GDK_IS_SEAT` failures. The crashing frame is not identified, because gdb does not run in the local VM. It is worth a guard, whatever the packaging decision.

## Not in this plan

- APT and DNF repositories with signing keys.
- Flatpak. It remains an option later; it would also remove the glibc question.
- NixOS. The prototype worked (GCC 15 on the glibc 2.39 package set), but it bundles a package set without security updates, and Debian 13 covers the same targets.
