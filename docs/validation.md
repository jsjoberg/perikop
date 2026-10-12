# Validation record

## Current implementation

Record date: 12 October 2026.
The local changes start from [e9da60c](https://github.com/jsjoberg/perikop/commit/e9da60c4caee597b69c1dc441720438ba4e1c068).
The [improvement tracker](improvement-plan.md) records the changes and remaining work.
The [10 October record](validation-2026-10-10.md) preserves the earlier measurements.

| Platform | Current minimum | Remaining execution checks |
| --- | --- | --- |
| macOS arm64 and x86-64 | macOS 13.4 | Intel, minimum-version hardware, physical playback |
| Windows x86-64 | Windows 10 | Installed package, minimum-version hardware, physical playback |
| Linux x86-64 | glibc 2.34 and GTK3 | Installed AppImage, target distributions, physical playback |

All platforms use portable Kokoro speech with Alice and Björn.
Corpus schema 7 and study schema 4 are current.
The release workflow includes builds for Windows, Linux, and both macOS architectures.
The workflow now requires ASan and UBSan checks before publication.
Package checks now generate audio with both bundled voices and both passage lengths.
A successful local check does not establish success on another platform.

## Local evidence

The local build uses Apple Clang 17 on macOS 26.6.2, arm64.
The release build passed all 10 CTest tests, including 26,379 core checks.
The Python checks passed 19 tests. All 74 resource hashes passed.
Formatting and static analysis also passed.
The full ASan/UBSan build passed all 10 tests with LLVM Clang 22.1.8 and the same native wxWidgets libraries.
Apple Clang 17's ASan runtime stalled during initialization on this macOS 26.6.2 host, before application code ran.
The local LLVM build used `-include cstddef` for compatibility with the installed SDK headers.
The CI sanitizer job uses the supported Apple toolchain on its macOS 15 runner and still needs its first run.
The CTest suite covers storage, calendar calculations, speech preparation, PCM streaming, tempo changes, optional cache recovery, and native UI behavior.
The tempo regressions cover reused stream addresses, unity-speed transitions, quarter-speed tails, and double-speed transitions.
The worker regression covers pause, resume, and a speed change during the final tempo tail.
Progress uses delivered output, with an estimated source position during tempo changes.
Completion waits for the tempo output to drain.
Physical device latency remains outside these component checks.

The GUI checks cover native plan buttons, custom keyboard activation, and preserved settings after optional pronunciation failures.
Diagnostic checks run both smoke tests and benchmarks with an isolated profile.
Sentinel files in an explicitly supplied personal profile remain unchanged.
The Windows/Linux wheel regression runs on those platforms because Cocoa does not use the wheel timer.

Real inference from `kokoro-sv-alice-bjorn-2c7968d-v3` passed for Alice and Björn on arm64.
The short passage produced 4.85 seconds for Alice and 4.825 seconds for Björn.
The long passage produced 65.95 seconds for Alice and 73.25 seconds for Björn.
Generation took approximately 0.95/12.52 seconds for Alice and 0.85/13.66 seconds for Björn (short/long).
Each result contained finite, non-silent PCM.
These measurements exclude physical audio playback and listening quality.

Cache fault checks use real inference and a simulated audio device.
They cover corrupt data, future schemas, missing tables, write rejection, database locks, and an invalid cache path.
Speech completes without the cache and preserves progress and pronunciation decisions.
The GUI checks also preserve corrupt pronunciation bytes and corrections in an unsupported future schema.
Full-disk and platform permission faults still need checks on installed packages.

## Independent calendar evidence

The existing fixtures compare the Antiochian 2026 Sunday chart and OCA daily readings for 2026.
The core suite retains explicit exceptions and calendar arithmetic checks.
No complete independent reference confirms the Greek daily order.
A complete-year Greek reference and a second year with different feast interactions remain necessary.
A domain reviewer must approve source-backed explanations for differences.
Algorithm agreement alone does not establish the intended jurisdiction's reading assignments.

## Remaining manual checks

VoiceOver and NVDA checks remain for control roles, toggle states, and Scripture text.
The custom Scripture panel still needs an accessible representation of its verses.
Listening checks remain for both voices, short and long readings, and pronunciation corrections.
Installed-package checks remain for Windows, Linux, Intel Macs, and minimum platform versions.
Overnight wake, multiple displays, and physical audio-device behavior also remain outside the local automated evidence.
Repository ruleset enforcement is a separate administration task.
