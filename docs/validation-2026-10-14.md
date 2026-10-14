# Antiochian chart audit, October 14, 2026

The app calendar begins January 1, 2019. Annual source data before 2019 are removed for both jurisdictions.
The native engine retains earlier arithmetic for its internal Paschal-cycle tests.

All Sundays in the official annual charts from 2019 through 2026 are transcribed and checked: 417 Epistle/Gospel pairs, preserving every segment.
The 2023 chart contains 53 Sundays; the other seven contain 52 each.
The earliest accessible chart located was 2019. The tested 2016–2018 chart URLs were unavailable.
The downloaded charts were extracted with Poppler and the two reading-table pages in each chart were visually checked.
The original 2026 fixture agrees with the new extraction, including the December 27 Acts chapter crossing.
Chart URLs occur on every imported reading. The manifest pins the original PDF SHA-256 hashes.

The published chart replaces the main Sunday Divine Liturgy pair for its exact civil date in New Calendar mode.
Additional saint readings remain calculated from Orthocal; matching readings already in the official pair are not duplicated.
Matins, Vespers, weekday readings, and Old Calendar mode remain calculated from Orthocal.
The chart does not establish that every reading omitted from its Sunday pair is omitted from all services.
The app uses the chart as the main Sunday Liturgy selection; additional readings and other services remain available.

The 2025 second Sunday after Pentecost uses Romans 2:10–16, rather than the 2026 All Saints of Antioch reading.
The March 9, 2025, Forty Martyrs Epistle is Hebrews 12:1–10.
The 2024 chart adds Luke 8:8 after Matthew 25:14–30 on February 11.
These are date-specific source assignments, not recurring overrides.

No accessible official Antiochian 2027 Sunday readings chart was located. Dates from 2027 onward remain calculated pending official publication.
The major-feast dates printed through 2030 in the 2026 chart do not constitute complete future reading charts.

Validation passed on macOS arm64: all 11 CTest tests, including the 417 Sunday pair comparisons in abbreviated and full service modes, the date-boundary checks, UI smoke, and profile isolation.
All 98 resource hashes and both bundled font coverage checks passed. Native formatting and `git diff --check` passed.
The rebuilt corpus contains 104 dated, sourced readings per chart year except 2023, which contains 106. No annual assignments precede 2019.

## Month and year navigation

The calendar header has separate month and year labels, each with backward and forward arrows.
Month arrows move one month; year arrows move twelve months while preserving the day where valid.
Shift+Page Up and Shift+Page Down also move by year. Leap-day jumps clamp to February 28.
Disabled arrows and keyboard navigation respect January 1, 2019, and December 31, 9999.
Mouse hit targets, tooltips, leap-day behavior, month jumps, keyboard year jumps, and both limits are covered by the GUI smoke test.

The expanded 2019–2026 suite passed all 11 CTest tests in 33.52 seconds on macOS arm64.
The isolated screenshot smoke test also passed. The September header, first supported year, and dark theme were visually inspected; labels and all four arrow targets fit without overlap.
All 98 resource hashes and native formatting passed after the expanded import.
