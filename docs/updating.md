# In-app updates

The updater, platform packages, and feed publication are integrated. Release targets are Windows x64, Linux x64, Linux ARM64, and Apple Silicon Macs running macOS 15 or later. The ARM64 package targets Raspberry Pi 3, 4 and 5 on 64-bit Raspberry Pi OS Trixie with a desktop. Intel Macs and macOS 13/14 are no longer release targets.

The macOS 15 baseline must pass the native update checks on the macOS 15 runner before release. The pinned Velopack SDK still contains crypto objects built for macOS 26.5; raising the deployment target does not establish runtime compatibility. Local checks on a newer Mac establish behavior only on that system.

The Pi models share the same ARM64 package and update feeds. Pi 3/4 compatibility is expected; physical-device performance and audio remain unverified. The glibc 2.39 boundary still applies independently of the speech engine. See [Raspberry Pi installation](using-perikop.md#raspberry-pi-3-4-and-5).

## User-controlled updates

- GitHub Releases in `jsjoberg/perikop-updates` hosts the manifests and packages from 0.2.1. Ordinary downloads stay in `jsjoberg/perikop`.
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

The SDK's GitHub source sends no installation identifier. Its [pinned implementation](https://github.com/velopack/velopack/blob/1.2.161/src/lib-rust/src/sources/github.rs) requests ten releases before filtering prereleases on the release track. Dependency snapshots therefore stay in `jsjoberg/perikop-assets`; application updates use dedicated `jsjoberg/perikop-updates`, and both application repositories keep at most seven nightlies. The HTTP source is not used because it adds a random per-installation staging identifier to feed requests.

Historical Intel Mac feeds remain readable by nightly cleanup so their referenced packages survive while those older nightlies are retained. New publications contain only the four supported targets.

## Migration from 0.2.0

The published `v0.2.0` application uses `GithubSource(https://github.com/jsjoberg/perikop)` and already uses the glibc 2.39 Linux channels. Its existing updater finds 0.2.1's compatibility feeds and full packages on the main release, verifies and installs the full update, then the replacement application's compiled source points to `https://github.com/jsjoberg/perikop-updates`. Subsequent versions are found there. Package identity, channel names, the updater state machine, profile paths, delta/full fallback, version ordering and user confirmation for rollback are unchanged. No uninstall or profile move is involved.

Nightly migration follows the same path using `*-nightly` feeds on a main-repository nightly containing the new updater. New partial nightlies preserve previously migrated platforms' full feeds/packages, even when those platforms are missing from the current build. Retired Linux feeds remain retired: this storage change does not make a glibc 2.39 AppImage safe for a pre-boundary client. Historical Intel Mac builds likewise do not gain a supported new target.

A single 0.2.1 bridge would eventually disappear from the old SDK's ten-release window. Publication refreshes main stable compatibility feeds at least every third stable release and copies nightly compatibility feeds when they would leave the retained window. This deliberately permits extra main-repository assets on bridge releases; deleting them early breaks late migrations. Primary filenames replace the internal track with a version, such as `Perikop-win-x64-0.2.1-Setup.exe`. Generated notes name each exact download.

The update repository carries every package referenced by its new feeds, including delta bases restored from the legacy source. Carried feeds include only their newest full package; historical deltas are not repeated. Both repositories are verified as drafts and the main release becomes public last. See [setup, publication and recovery](releasing.md).

## Packaging and validation

`tools/ci/build.py` runs the native checks and packaging for each target. Python tooling is prepared once per job through uv, then Linux containers and macOS sandboxed builds run with networking disabled. Windows compilation runs on Linux; Windows receives the binaries, runs native tests, and packages them without recompiling. See [building](building.md).

Full and delta update tests exercise the real application's `--update-probe` backend using local feeds. They verify replacement and profile preservation. The delta test omits the target full package, forcing actual reconstruction. Linux traces Internet sockets; macOS denies networking; Windows does not trace network calls.

The macOS job packages only ARM64 and saves `macos-update-validation-arm64.zip`. Its self-contained kit requires only macOS tools. On an Apple Silicon Mac running macOS 15, run:

```sh
bash /path/to/validate_macos_update.command --require-macos15
```

The script verifies kit checksums, creates a private HOME/profile/installation, extracts the first PKG, applies a full update and then a delta, verifies architecture, signatures, deployment target, UI, and profile preservation, and removes the private application. It saves `result-arm64.txt` and retains diagnostic files on failure. A pass on a newer macOS is useful but does not establish the macOS 15 minimum. The check does not exercise Installer's privileged scripts, Gatekeeper, or HTTPS against a published feed.

Packages remain unsigned by Developer ID or Authenticode and are not notarized. The macOS bundle uses an ad-hoc signature. From 0.2.0, releases and nightlies carry only the Velopack packages. A 0.1.x copy from the DMG, the plain AppImage or the NSIS installer cannot update itself: install the Velopack package once by hand. Windows users uninstall the NSIS version first; their profile stays in place.

`tools/ci/update_packages.py` verifies previous same-channel full packages before offline packing, collects verified bases and the new full package and delta, validates feed versions and platform coverage, and checksums staging. `tools/ci/publish_release.py` splits versioned downloads from internal assets, preserves migration feeds and runtime sources, verifies remote bytes, and promotes paired releases. A channel's first package has no delta. Tag builds create drafts in both repositories; nightlies promote their paired releases after verification. Manual non-nightly workflow runs produce artifacts without publishing. See [releasing](releasing.md) for retention and publication details.

`tests/publish_release_test.py` uses local bytes and a simulated GitHub API to check old-source migration, new-source discovery, all platforms, separate channels, full/delta availability, periodic bridge refresh, partial-nightly feeds, source retention, scoped credentials and failure ordering. `tests/update_packages_test.py` covers checksums, delta-base restoration, glibc boundaries and retention rescue. These are fixture tests, not native installation or published-HTTPS tests. The existing native full/delta update probes remain required in CI, including their profile-preservation checks.

## Earlier compatibility research

The standalone [Linux ARM64 prototype](reports/velopack-linux-2026-10-16.md) established static SDK compatibility with glibc 2.34. The shared Linux SDK requires glibc 2.39, so production Linux builds use static linkage. The Debian 13 build is held to glibc 2.39, the Enterprise Linux 10 version, which the shared SDK would also meet; the static linkage stays. Alpine/musl is not a target.

The [October 18 macOS report](reports/velopack-macos-2026-10-18.md) records earlier ARM64/Rosetta checks and the former macOS 13 validation gap. Those historical results do not establish the new macOS 15 baseline. macOS builds and packaging remain native; cross-compiling macOS from Linux is outside the release plan.
