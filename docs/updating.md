# In-app updates

The updater, platform packages, and feed publication are integrated. Supported release targets are Windows x64, Linux x64, Linux ARM64 for Raspberry Pi 5, and Apple Silicon Macs running macOS 15 or later. Intel Macs and macOS 13/14 are no longer release targets.

The macOS 15 baseline must pass the native update checks on the macOS 15 runner before release. The pinned Velopack SDK still contains crypto objects built for macOS 26.5; raising the deployment target does not establish runtime compatibility. Local checks on a newer Mac establish behavior only on that system.

## User-controlled updates

- GitHub Releases hosts the manifests and packages. No additional update server is used.
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

Each channel is `<os>-<arch>-<track>`, with `-glibc241` after the architecture on Linux:

| OS | Architectures | Tracks |
| --- | --- | --- |
| `win` | `x64` | `releases`, `nightly` |
| `linux` | `x64`, `arm64` | `releases`, `nightly` |
| `osx` | `arm64` | `releases`, `nightly` |

For example, `linux-arm64-glibc241-nightly` uses `releases.linux-arm64-glibc241-nightly.json`. The application knows its platform; the preference chooses only the track.

Debian 13 AppImages publish only to `linux-{x64,arm64}-glibc241-{releases,nightly}`. The earlier `linux-{x64,arm64}-{releases,nightly}` feeds receive no new builds. An earlier client therefore cannot select an incompatible AppImage, even if it runs on a host with older glibc. On a supported system, download the new AppImage once to move to the new feeds; the reading profile stays in place. Delta bases come only from the new feed. Historical feeds and their referenced packages remain valid for nightly retention.

This separation is required because the pinned Velopack 1.2.161 Linux updater replaces the AppImage before starting it, without checking glibc compatibility or restoring the previous image after a failed start. Changing only the new application's compatibility check would not protect existing clients.

The SDK's GitHub source sends no installation identifier. Its pinned implementation reads the ten newest releases and filters prereleases on the release track. Dependency snapshots therefore live in `jsjoberg/perikop-assets`, and cleanup keeps seven nightlies. The HTTP source is not used because it adds a random per-installation staging identifier to feed requests.

Historical Intel Mac feeds remain readable by nightly cleanup so their referenced packages survive while those older nightlies are retained. New publications contain only the four supported targets.

## Packaging and validation

`tools/ci/build.py` runs the native checks and packaging for each target. Python tooling is prepared once per job through uv, then Linux containers and macOS sandboxed builds run with networking disabled. Windows compilation runs on Linux; Windows receives the binaries, runs native tests, and packages them without recompiling. See [building](building.md).

Full and delta update tests exercise the real application's `--update-probe` backend using local feeds. They verify replacement and profile preservation. The delta test omits the target full package, forcing actual reconstruction. Linux traces Internet sockets; macOS denies networking; Windows does not trace network calls.

The macOS job packages only ARM64 and saves `macos-update-validation-arm64.zip`. Its self-contained kit requires only macOS tools. On an Apple Silicon Mac running macOS 15, run:

```sh
bash /path/to/validate_macos_update.command --require-macos15
```

The script verifies kit checksums, creates a private HOME/profile/installation, extracts the first PKG, applies a full update and then a delta, verifies architecture, signatures, deployment target, UI, and profile preservation, and removes the private application. It saves `result-arm64.txt` and retains diagnostic files on failure. A pass on a newer macOS is useful but does not establish the macOS 15 minimum. The check does not exercise Installer's privileged scripts, Gatekeeper, or HTTPS against a published feed.

Packages remain unsigned by Developer ID or Authenticode and are not notarized. The macOS bundle uses an ad-hoc signature. From 0.2.0, releases and nightlies carry only the Velopack packages. A 0.1.x copy from the DMG, the plain AppImage or the NSIS installer cannot update itself: install the Velopack package once by hand. Windows users uninstall the NSIS version first; their profile stays in place.

`tools/ci/update_packages.py` verifies previous same-channel full packages before offline packing, collects the new full package and delta, validates feed versions and platform coverage, and checksums the complete publication. A channel's first package has no delta. Tag builds create drafts; nightlies publish successful platforms together with their feeds. Manual non-nightly workflow runs produce artifacts without publishing. See [releasing](releasing.md) for retention and publication details.

## Earlier compatibility research

The standalone [Linux ARM64 prototype](reports/velopack-linux-2026-10-16.md) established static SDK compatibility with glibc 2.34. The shared Linux SDK requires glibc 2.39, so production Linux builds use static linkage. The Debian 13 build image now sets a glibc 2.41 baseline, which the shared SDK would also meet; the static linkage stays. Alpine/musl is not a target.

The [October 18 macOS report](reports/velopack-macos-2026-10-18.md) records earlier ARM64/Rosetta checks and the former macOS 13 validation gap. Those historical results do not establish the new macOS 15 baseline. macOS builds and packaging remain native; cross-compiling macOS from Linux is outside the release plan.
