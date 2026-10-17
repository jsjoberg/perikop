# In-app updates and build direction

This is the implementation plan for the Velopack migration. The saved update-channel setting, separate macOS architecture packages, and ordered nightly versions exist; Velopack and the in-app update checker are not integrated yet. [Phases](#phases) lists the steps.

## Linux as the primary build host

Implement and validate the updater on Linux first. Keep the shared updater and UI portable across Windows, macOS, and Linux. Prefer Linux for compilation, packaging, feed generation, and release publication wherever the tools support it.

| Target | Planned compilation and packaging | Native validation |
| --- | --- | --- |
| Linux x64 | Linux | Linux |
| Linux ARM64 / Raspberry Pi 5 | Linux ARM64 | Linux ARM64 |
| Windows x64 | Cross-compile and create the Velopack EXE on Linux | Windows |
| macOS Apple Silicon | macOS initially; cross-compilation on Linux is the longer-term direction | macOS Apple Silicon |
| macOS Intel | macOS initially; cross-compilation on Linux is the longer-term direction | macOS / Rosetta, with Intel hardware validation retained |

[Velopack supports creating Windows and Linux packages on Linux](https://docs.velopack.io/packaging/cross-compiling). Its documented macOS packaging requires macOS tools, so moving Mac compilation to Linux does not by itself move final Mac packaging to Linux.

Keep compilation, resource staging, signing, packaging, and native validation as separate steps. Native validation must consume the transferred binaries without rebuilding them, following the existing Windows handoff model. For Windows, account for the app-local MSVC runtime DLLs currently supplied by the Windows job when assembling the Linux packaging payload.

The first compatibility prototype should run in the pinned Linux build environments and verify:

- Native Velopack SDK support on both Linux architectures, including compatibility with the existing glibc 2.34 baseline.
- Packaging the existing GTK AppDir without losing its bundled libraries, licence notices, or native Wayland behavior. Velopack accepts a staged directory ending in `.AppDir`; see [Linux packaging](https://docs.velopack.io/packaging/operating-systems/linux).
- Updating from one locally prepared version to another, preserving the user profile, with startup networking and automatic application of downloaded updates disabled.
- Producing a Windows EXE on Linux from the existing cross-built application and validating it on Windows.

### Linux compatibility prototype

The separate `Velopack Linux compatibility prototype` workflow is manual (`workflow_dispatch`) and runs on Linux x64 and ARM64. It does not publish releases or change Perikop's installers. It pins Velopack 1.2.161 and verifies the SDK, packaging tool, and licence hashes in `ci/velopack-prototype.json`.

To run it on a Linux host with Docker and Python 3.12 or later:

```sh
python3 tools/ci/velopack_prototype.py prepare
docker build --pull -f ci/velopack-prototype.Dockerfile -t perikop-velopack-prototype ci
docker run --rm --pull never --network none --cap-add SYS_PTRACE -v "$PWD:/work" \
    perikop-velopack-prototype python3.12 tools/ci/velopack_prototype.py run
```

Preparation downloads the explicitly pinned upstream inputs; the test environment installs its build tools during preparation. The compile/package/update test itself runs without networking on Rocky Linux 9 (glibc 2.34), using local update feeds. This research workflow is separate from ordinary offline CI; promote successful inputs into dependency snapshots before enabling production integration.

The probe links the native SDK statically against the baseline system libraries and records whether the upstream shared SDK can load. It creates a small native AppImage, updates it from 1.0.0 to 1.0.1, switches to a newer nightly, and returns to the older release. It checks that a profile fixture outside the AppImage survives, and that starting with a staged update neither applies it nor attempts Internet networking. The production GUI and its existing profile databases are not part of this probe.

Diagnostic logs, syscall traces, and `summary.json` are written under `build/velopack-prototype/run-<architecture>`. Each run rebuilds that architecture's prototype output. Windows cross-packaging, GTK AppDir integration, actual application upgrades, and native Mac/Windows validation remain subsequent steps.

The [October 16 Linux ARM64 findings](reports/velopack-linux-2026-10-16.md) record the successful offline upgrade and channel-switch tests, static SDK compatibility with glibc 2.34, and the SDK's cleanup of cached downgrades at startup.

Pin the SDK and packaging tools and prepare them through the dependency-snapshot workflow. Ordinary compilation and packaging remain offline. Signing, notarization, and publication are explicit release steps with their own network access where necessary.

## macOS cross-compilation later

[OSXCross](https://github.com/tpoechtrager/osxcross) supports targeting both `arm64` and `x86_64` from Linux. Before moving the production Mac compile jobs, prove that a pinned toolchain and macOS SDK can build Perikop's C++23 code, Objective-C++ Cocoa integration, wxWidgets, and both ONNX Runtime variants.

Transfer each resulting app bundle to a Mac for GUI and speech tests, signing, notarization, and final packaging. Keep the existing native Mac builds until the cross-built versions pass the same checks. Evaluate moving final packaging and signing off macOS separately; do not assume that OSXCross makes Velopack's Mac packaging portable.

## User-controlled updates

- GitHub Releases hosts the manifests and packages. No additional update server is planned.
- Releases and Nightlies each have separate feeds for every supported OS and architecture. Application feeds exclude dependency snapshots and voice-pack releases.
- The channel preference survives restarts. Changing it only saves the preference.
- Only an explicit **Check for updates** action initiates the check. Downloading and installing require separate user actions.
- No startup checks, polling, background downloads, telemetry, or automatic application of staged updates.
- Run requests off the UI thread and support cancellation. Offline operation and update failures must not disrupt reading or speech.
- Give nightlies increasing versions, retaining the commit identity for display. Returning to an older release requires explicit confirmation and profile compatibility checks.
- Keep settings and personal data outside replaceable application files, preserving the existing profile location.

The shared UI should expose checking, available-version details, downloading, and install/restart states. Platform-specific installation code sits behind that shared interface. Test real upgrades, interrupted downloads, channel changes, profile preservation, and the absence of unsolicited network requests on every target before replacing the current installers.

## Versions and channels

Nightlies have SemVer versions that Velopack can order. Release tags keep their versions, such as `0.1.0` or `0.2.0-alpha.1`.

- Before the version in `CMakeLists.txt` is released, a nightly is a prerelease of it: `0.2.0-nightly.20261101.500.1`. It sorts above `0.2.0-alpha.1` and below `0.2.0`.
- After that version is released, a nightly is a prerelease of the next patch: `0.1.1-nightly.20261018.412.1`. It sorts above `0.1.0`.
- The numbers after `nightly` are the Stockholm date, the workflow run number, and the run attempt. Later nightlies, and retries of the same night, always sort higher.

The `nightly` job chooses the version with `tools/ci/nightly.py version` and passes it to every build job as `PERIKOP_BUILD_VERSION`. The build uses it instead of the computed version, and the version header records the commit as `PERIKOP_COMMIT`. **Om Perikop** shows a version such as `0.1.1-nightly.20261018.412.1 (b250eaa)`. Builds outside nightlies keep their computed versions. Nightly tags and download file names are unchanged.

Each package belongs to one channel, named `<os>-<arch>-<track>`:

| OS | Architectures | Tracks |
| --- | --- | --- |
| `win` | `x64` | `releases`, `nightly` |
| `linux` | `x64`, `arm64` | `releases`, `nightly` |
| `osx` | `arm64`, `x64` | `releases`, `nightly` |

For example, `linux-arm64-nightly` is the feed `releases.linux-arm64-nightly.json`. The update-channel setting chooses the track; the program knows its OS and architecture.

## Decisions

- **Signing:** packages stay unsigned for now, as today. The macOS installer meets Gatekeeper and the Windows Setup.exe meets SmartScreen, as the current DMG and NSIS installer do. Developer ID and Authenticode signing are a later, separate release step.
- **Cancelling a download:** the Velopack 1.2.161 C API reports download progress but cannot cancel. **Avbryt** stops waiting and discards the result. A download the user started may finish in the background, and the SDK removes stale packages at the next start. A cancellable downloader would need our own HTTP code on every platform.
- **Existing packages:** the plain AppImage, DMG, and NSIS installer stay in releases and nightlies until each platform's Velopack package passes its native checks. Windows users uninstall the NSIS version once by hand. The profile is outside the program files and stays.
- **Delta updates:** enable them. Each full package is 170–215 MB, mostly the voice pack, which rarely changes. CI builds a delta from the previous package in the same channel.

### Open: where the program reads the feed

The two built-in update sources in the pinned SDK each have a problem for this repository:

- **GitHub source** (`vpkc_new_source_github`) reads only the 10 newest releases from the GitHub API, then merges their feeds; for the releases track it drops prereleases first. This repository also publishes up to seven nightlies, CI dependency snapshots, and voice packs as prereleases. The newest stable release can fall outside those 10, and the releases track would then find no update. The source sends no identifier.
- **HTTP source** (`vpkc_new_source_http_url`) reads `releases.<channel>.json` from one base URL and resolves package names against it, so a feed can point at packages in other releases with absolute URLs. For the releases track, `https://github.com/<owner>/<repo>/releases/latest/download/` always names the newest stable release. Nightlies need a fixed release that holds the current nightly feeds. Each request adds `localVersion`, the app ID, and a random per-installation `stagingId` to the feed URL, so GitHub receives a persistent identifier with every check.

To use the GitHub source, move CI dependency snapshots and voice packs to another repository and keep at most nine nightlies. To use the HTTP source, accept the identifier in the query string. Choose before phase 2. See the pinned [GitHub source](https://github.com/velopack/velopack/blob/92d6a1c91716729d449034df5c50307dcce39493/src/lib-rust/src/sources/github.rs) and [HTTP source](https://github.com/velopack/velopack/blob/92d6a1c91716729d449034df5c50307dcce39493/src/lib-rust/src/sources/http.rs).

## Phases

Each phase is its own pull request.

1. Versions, channels, and these decisions. Done.
2. Updater core and UI: a small updater interface over the Velopack C API, with a fake for tests. It runs off the UI thread. `VelopackApp` runs at startup. **Inställningar** gets **Sök efter uppdateringar**, which checks, shows the version, downloads with progress, and offers **Starta om och installera**. Development builds say that updates need an installed copy.
3. Linux: pack the AppDir with `vpk` on x64 and ARM64, with the static SDK. Test a real upgrade and the profile offline on the glibc 2.34 baseline.
4. Windows: pack Setup.exe on Linux and test install, update, and uninstall on a Windows runner. The SDK's Windows libraries are MSVC builds; with llvm-mingw, link its DLL through the C API.
5. macOS: pack each architecture with `vpk` on the Mac runners.
6. Publishing: releases and nightlies upload feeds, full packages, and deltas, and nightly cleanup keeps the packages that deltas need.
