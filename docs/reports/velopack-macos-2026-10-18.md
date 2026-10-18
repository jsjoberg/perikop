# macOS Velopack validation, October 18, 2026

Velopack 1.2.161 is pinned in the dependency snapshot. The application targets macOS 13.4, but its static SDK contains crypto objects built for macOS 26.5. Linker warnings remain visible. Deployment-target metadata alone cannot prove these objects run on macOS 13.

## Available checks

On macOS 26.6.2 Apple Silicon, the ARM64 application was built against the pinned SDK and packaged with `vpk`. The real application updated from 1.0.0-test.1 to 1.0.0-test.2 from a local feed with networking denied. The updated bundle passed strict signature verification and the UI smoke test, and settings and reading progress survived its replacement and removal. These results establish behavior on this system only.

The packaging helper stages the existing bundle, preserves `LSMinimumSystemVersion` and `CFBundleIdentifier`, updates version metadata, and gives `vpk` a private copy of `UpdateMac` thinned to the selected architecture. Ad-hoc signing renews the bundle seal after the updater and `sq.version` are added. No Developer ID credentials, timestamp service, or notarization service are used.

The automated check extracts a PKG payload into a temporary installation, runs the same backend as the update UI, and checks the replaced bundle and profile. It does not execute Apple's Installer UI, the PKG's privileged postinstall script, or Gatekeeper's handling of a downloaded unsigned installer. The original DMG remains available.

## macOS 13 baseline: pending

No macOS 13 machine or VM is available in this session. No macOS 13 compatibility claim has been made.

CI provides a validation ZIP for each architecture. Extract it on macOS 13.4 or later in the 13 series and run:

```sh
bash /path/to/validate_macos_update.command --require-macos13
```

It needs neither Python, .NET, a compiler, administrator access, nor networking. The script checks the OS version, validates kit checksums, creates a private HOME/profile/installation, extracts the first PKG, checks/downloads/applies the second version using the real SDK, verifies the resulting signature, architecture and deployment target, runs a UI smoke check, and removes the private application. The profile remains intact. It writes `result-<architecture>.txt` beside the script and retains temporary diagnostic files on failure.

Record OS version, hardware architecture, whether Rosetta was used, both package hashes, and the saved result before closing the baseline check. Native Intel hardware validation remains separate from an Intel test through Rosetta. The local-feed test exercises package validation and replacement; HTTPS/TLS behavior will also need a baseline check against the published feed when phase 6 makes it available.
