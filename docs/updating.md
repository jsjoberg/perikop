# In-app updates

The updater, platform packages, and feed publication are integrated. Release targets are Windows x64, Linux x64, Linux ARM64, and Apple Silicon Macs running macOS 15 or later. The ARM64 package targets Raspberry Pi 3, 4 and 5 on 64-bit Raspberry Pi OS Trixie with a desktop. Intel Macs and macOS 13/14 are no longer release targets.

The macOS 15 baseline must pass the native update checks on the macOS 15 runner before release. The pinned Velopack SDK still contains crypto objects built for macOS 26.5; raising the deployment target does not establish runtime compatibility. Local checks on a newer Mac establish behavior only on that system.

The Pi models share the same ARM64 package and update feeds. Pi 3/4 compatibility is expected; physical-device performance and audio remain unverified. The glibc 2.39 boundary still applies independently of the speech engine. See [Raspberry Pi installation](using-perikop.md#raspberry-pi-3-4-and-5).

## User-controlled updates

- From 0.2.1, the `updates` release in `jsjoberg/perikop-assets` holds every feed and package. Downloads stay in `jsjoberg/perikop`.
- Releases and Nightlies have separate feeds for each supported OS and architecture. Application feeds exclude dependency snapshots.
- Changing the channel only saves the preference. Only **Check for updates** initiates a check; downloading and installing require separate actions.
- No startup checks, polling, background downloads, telemetry, or automatic application of staged updates.
- Worker threads keep checking, downloading, and applying off the UI thread. Offline operation and update failures must not disrupt reading or speech.
- Settings, completion marks, and passage positions stay in the existing profile outside replaceable application files.
- Returning to an older release requires explicit confirmation and profile compatibility checks.

Velopack 1.2.161's C API cannot cancel a download in progress. **Avbryt** stops waiting and discards its result; that user-started download may finish in the background. No new update step starts until it has finished. Startup may clean cached packages but never downloads or applies them.

`src/core/updater.cpp` owns the shared state machine. `src/app/updates.cpp` implements the Velopack backend, retaining the selected package through check, download, and apply. The SDK runs its startup lifecycle only in a Velopack-installed copy. It is linked statically on Linux and macOS, and through its DLL on Windows.

## Versions and channels

Release tags keep their SemVer versions, such as `0.1.0` or `0.2.0-alpha.1`. Nightlies use increasing versions:

- Before the CMake version is released: `0.2.0-nightly.20261101.500.1`, above `0.2.0-alpha.1` and below `0.2.0`.
- After it is released: `0.1.1-nightly.20261018.412.1`, above `0.1.0`.
- The nightly suffix contains the Stockholm date, workflow run number, and attempt. About also shows the commit identity.

Each channel is `<os>-<arch>-<track>`, with `-glibc239` after the architecture on Linux:

| OS | Architectures | Tracks |
| --- | --- | --- |
| `win` | `x64` | `releases`, `nightly` |
| `linux` | `x64`, `arm64` | `releases`, `nightly` |
| `osx` | `arm64` | `releases`, `nightly` |

For example, `linux-arm64-glibc239-nightly` uses `releases.linux-arm64-glibc239-nightly.json`. The application knows its platform; the preference chooses only the track.

Debian 13 AppImages publish only to `linux-{x64,arm64}-glibc239-{releases,nightly}`. The earlier `linux-{x64,arm64}-{releases,nightly}` feeds receive no new builds. An earlier client therefore cannot select an incompatible AppImage, even if it runs on a host with older glibc. On a supported system, download the new AppImage once to move to the new feeds; the reading profile stays in place. Delta bases come only from the new feed. Historical feeds and their referenced packages remain valid for nightly retention.

This separation is required because the pinned Velopack 1.2.161 Linux updater replaces the AppImage before starting it, without checking glibc compatibility or restoring the previous image after a failed start. Changing only the new application's compatibility check would not protect existing clients.

The application reads its feeds through the SDK's HTTP source, from `https://github.com/jsjoberg/perikop-assets/releases/download/updates`. The source fetches `releases.<channel>.json` there and downloads each package by its name from the same place, so feeds and packages share one fixed release. Its [pinned implementation](https://github.com/velopack/velopack/blob/1.2.161/src/lib-rust/src/sources/http.rs) adds `localVersion`, the application `id` and `stagingId` to the feed request. The staging ID is a random UUID that the SDK creates once per installation and keeps in its packages directory; [its options](https://github.com/velopack/velopack/blob/1.2.161/src/lib-cpp/include/Velopack.hpp) cannot turn it off. GitHub receives it with the user's IP address; this project cannot see GitHub's request logs.

The GitHub source, which 0.2.0 used, reads only the ten newest releases of a repository. The HTTP source reads one file and has no such window.

Each feed holds the channel's newest full package and the deltas of the seven newest versions. The SDK applies every delta above the installed version, so a copy older than the oldest delta's base gets an incomplete chain. The SDK downloads the full package only if patching fails; it does not check the patched package as a whole. The GitHub source's ten-release window had the same limit. Packages that no live or staged feed names are deleted when a feed is published. See [releasing](releasing.md).

New publications contain only the four supported targets.

## Migration from 0.2.0

The published `v0.2.0` application uses `GithubSource(https://github.com/jsjoberg/perikop)` and already uses the glibc 2.39 Linux channels. The `v0.2.1` release in `jsjoberg/perikop` therefore also carries the four release feeds with their full packages and deltas. The 0.2.0 updater finds them there and installs 0.2.1, whose compiled source is the HTTP source above. Package identity, channel names, the updater state machine, profile paths, delta/full fallback, version ordering and user confirmation for rollback are unchanged. No uninstall or profile move is involved.

This copy is one-time. A 0.2.0 copy that waits until `v0.2.1` leaves the ten newest releases must be reinstalled once. Nightly copies from before 0.2.1 get no copy: install a new nightly once. Retired Linux feeds remain retired.

## Packaging and validation

`tools/ci/build.py` runs the native checks and packaging for each target. Python tooling is prepared once per job through uv, then Linux containers and macOS sandboxed builds run with networking disabled. Windows compilation runs on Linux; Windows receives the binaries, runs native tests, and packages them without recompiling. See [building](building.md).

Full and delta update tests exercise the real application's `--update-probe` backend using local feeds. They verify replacement and profile preservation. The delta test omits the target full package, forcing actual reconstruction. Linux traces Internet sockets; macOS denies networking; Windows does not trace network calls.

The macOS job packages only ARM64 and saves `macos-update-validation-arm64.zip`. Its self-contained kit requires only macOS tools. On an Apple Silicon Mac running macOS 15, run:

```sh
bash /path/to/validate_macos_update.command --require-macos15
```

The script verifies kit checksums, creates a private HOME/profile/installation, extracts the first PKG, applies a full update and then a delta, verifies architecture, signatures, deployment target, UI, and profile preservation, and removes the private application. It saves `result-arm64.txt` and retains diagnostic files on failure. A pass on a newer macOS is useful but does not establish the macOS 15 minimum. The check does not exercise Installer's privileged scripts, Gatekeeper, or HTTPS against a published feed.

Packages remain unsigned by Developer ID or Authenticode and are not notarized. The macOS bundle uses an ad-hoc signature. From 0.2.0, releases and nightlies carry only the Velopack packages. A 0.1.x copy from the DMG, the plain AppImage or the NSIS installer cannot update itself: install the Velopack package once by hand. Windows users uninstall the NSIS version first; their profile stays in place.

`tools/ci/update_packages.py` verifies previous same-channel full packages before offline packing, collects verified bases and the new full package and delta, validates feed versions and platform coverage, and gives the downloads their plain names. It stages packages and feed entries on the `updates` release and merges them into the live feeds when a release is published. A channel's first package has no delta. Tag builds create a draft; nightlies publish at once. Manual non-nightly workflow runs produce artifacts without publishing. See [releasing](releasing.md).

`tests/update_packages_test.py` covers checksums, delta-base restoration from the `updates` release and the main repository, glibc boundaries, plain download names, the one-time 0.2.1 copy, staging, publication, feed trimming and package deletion against a simulated `updates` release. These are fixture tests, not native installation or published-HTTPS tests. The existing native full/delta update probes remain required in CI, including their profile-preservation checks.

## Earlier compatibility research

The standalone [Linux ARM64 prototype](reports/velopack-linux-2026-10-16.md) established static SDK compatibility with glibc 2.34. The shared Linux SDK requires glibc 2.39, so production Linux builds use static linkage. The Debian 13 build is held to glibc 2.39, the Enterprise Linux 10 version, which the shared SDK would also meet; the static linkage stays. Alpine/musl is not a target.

The [October 18 macOS report](reports/velopack-macos-2026-10-18.md) records earlier ARM64/Rosetta checks and the former macOS 13 validation gap. Those historical results do not establish the new macOS 15 baseline. macOS builds and packaging remain native; cross-compiling macOS from Linux is outside the release plan.
