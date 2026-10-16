# Velopack Linux compatibility prototype

Standalone research on October 16, 2026. Perikop's production updater and release pipeline have not been migrated.

## Environment and inputs

- Linux ARM64 in a local Podman VM; Rocky Linux 9 container with glibc 2.34.
- A small C++17 program using Velopack's native C++ SDK. Perikop's GUI and C++23 build were not part of this test.
- Velopack SDK and packaging tool 1.2.161, verified against the SHA-256 pins in `ci/velopack-prototype.json`.
- .NET 8 for packaging only. The test application links the native SDK statically.
- Test execution uses `--network none`, local update feeds, and AppImage extraction mode rather than FUSE.

## Results

| Check | Linux ARM64 result |
| --- | --- |
| Load the upstream shared SDK on glibc 2.34 | Fails: requires `GLIBC_2.39` |
| Link the static SDK against the baseline system libraries | Passes; executable's maximum glibc requirement is 2.34 |
| Start the statically linked native program | Passes |
| Create and launch a native AppImage with `vpk` on Linux | Passes |
| Upgrade 1.0.0 to 1.0.1 | Passes |
| Switch 1.0.1 to 1.0.2-nightly.1 | Passes |
| Explicitly return from the nightly to release 1.0.1 | Passes |
| Keep an external profile fixture through each replacement | Passes |
| Start without applying a downloaded update | Passes, including a staged older release |
| Attempt Internet networking at startup | No AF_INET/AF_INET6 calls in the initial and staged-update syscall traces |
| Check when already on the latest release | No update offered |

## SDK lifecycle details

For this SDK release, use static linkage as the Linux compatibility candidate. The supplied Linux shared libraries reference glibc 2.39; the ARM64 loader failure was reproduced on the baseline system. Linking the static archive against glibc 2.34 produces a working baseline-compatible executable. Linux x64 still needs its own runtime verification.

`SetAutoApplyOnStartup(false)` prevents automatic application but does not disable package-cache cleanup. SDK startup retains the current and newer pending packages, removing older ones. A downloaded downgrade therefore does not survive another startup. This was reproduced by staging release 1.0.1 while running the newer nightly, starting the app again, and then finding the staged package unavailable. The behavior is visible in the pinned [SDK startup implementation](https://github.com/velopack/velopack/blob/92d6a1c91716729d449034df5c50307dcce39493/src/lib-rust/src/app.rs).

The pending-restart helper also excludes older releases. The successful probe holds the selected `UpdateInfo` through explicit download and apply in one SDK session and calls `WaitExitThenApplyUpdates` with that selected target. The future GUI should retain the selected target across those actions. After reopening the app, it must revalidate cached availability and obtain an explicit download action if the selected package is no longer present; it must never silently fetch it at startup.

## Reproduction and remaining work

Run the commands in [the migration plan](../updating.md#linux-compatibility-prototype). The manual `velopack-prototype.yml` workflow covers native Linux x64 and ARM64 runners, but has not been dispatched on GitHub. The local runtime evidence covers ARM64 only. Logs, ABI checks, four startup syscall traces, and `summary.json` are under `build/velopack-prototype/run-arm64`.

Still required before production integration: native Linux x64 verification, GTK AppDir/Wayland/library staging, real Perikop profile and upgrade checks, explicit restart and interruption behavior, shared GUI integration, GitHub feed discovery, Windows packaging from Linux and native validation, and macOS integration/signing. The prototype validates full packages; delta updates were disabled. Promote the chosen SDK and packaging tools into dependency snapshots before using them in ordinary builds.
