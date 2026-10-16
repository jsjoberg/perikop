# In-app updates and build direction

This is the implementation plan for the Velopack migration. The saved update-channel setting and separate macOS architecture packages exist; Velopack and the in-app update checker are not integrated yet.

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
