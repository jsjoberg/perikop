# Improvement tracker — 12 October 2026

This tracker follows the action report for revision `e9da60c`.
It separates code changes from checks that need other platforms, independent sources, or repository administration.
The [validation record](validation.md) records local evidence and its limits.

| Action | Changes in this patch | Remaining work |
| --- | --- | --- |
| A01: stale audio | Unique PCM session identities. Continuous Sonic processing across unity speed. Regressions for reused addresses and 25–200% transitions. | Physical playback checks. |
| A02: completion | Completion waits for tempo drain. Progress follows delivered output. Worker regression covers pause/resume and a final speed change. | Source position remains an estimate during tempo changes. Check physical device latency. |
| A03: keyboard access | Native plan buttons. Focus, Enter/Space activation, and visible focus for custom controls. Windows button roles and toggle states. | VoiceOver/NVDA review. Accessible Scripture verses. |
| A04: diagnostics | One injected directory for all writable stores. `--profile` for normal sessions. Both diagnostic modes use temporary profiles. | None in the implementation scope. |
| A05: optional data | Pronunciation recovery preserves files and uses bundled pronunciations. Cache errors permit uncached synthesis. Fault checks preserve personal data and future schemas. | Installed-package full-disk and permission checks. |
| A06: wheel conflict | Immediate navigation stops the timer and synchronizes its target. Stale timer events are ignored. Native Windows/Linux regression added. | Run the regression on Windows and Linux CI. |
| A07: bundled voices | Device-free short/long probes for Alice and Björn. Package checks use assembled resources and both macOS slices. Local arm64 inference passes. | Run the changed CI workflow. Complete the manual release matrix. |
| A08: Greek readings | Existing assurance gap remains explicit. Calendar assignments are unchanged. | Obtain authoritative complete-year fixtures for two distinct years and arrange domain review. |
| A09: required checks | Shared sanitizer flags cover storage, speech, tempo, and UI/layout code. A sanitizer job gates publication. | Configure and exercise a main-branch ruleset. Repository settings remain unchanged. |
| A10: docs | Current platform minima, portable speech, adaptive buffering, and schema versions agree. Earlier measurements have a separate archive. | Record results from the changed platform workflows. |

## Main-branch enforcement

After the changed workflow passes, select its actual Windows, Linux, macOS, and sanitizer check names in a main-branch ruleset.
Require those checks before merges. Block force pushes and branch deletion.
Choose review requirements for the actual contributor count and record any maintainer bypass.
Demonstrate that a failing required check prevents an ordinary merge.

## Greek daily reference

Retain the publisher, jurisdiction, year, retrieval date, and source file for each independent fixture.
Compare exact passages and service labels for every day.
Select the second year for different Pascha and feast interactions.
Record source-backed explanations for each difference and obtain domain approval.
Another implementation of the same inherited algorithm cannot serve as the sole reference.
