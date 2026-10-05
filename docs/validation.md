# Validation record

Local check date: October 6, 2026.
The release build used Apple Clang 17 on macOS 26.6.2, on Apple Silicon.
The application targets macOS 11.0. wxWidgets 3.3.3 and SQLite 3.53.4 link statically.

The core suite passed 615 checks.
The suite compared all 52 Sunday reading pairs with the Antiochian 2026 chart.
It also checked the October 5–6 daily references and official Pascha dates for 2026–2030.
Computed Pascha readings passed for 2027–2035.
Other checks cover date stability, Julian century leap labels, discontinuous passages, complete corpus endpoints, alignment, pronunciation, and local settings.

The GUI smoke test passed in all three themes and four parallel modes.
It also passed in a narrow window.
The test checks native paragraph justification, optical punctuation, final-line treatment, source-text recovery, and hyphenation in all three languages.
It checks fractional scrolling and compares images before and after a 17-pixel scroll.
At least 99.5% of the compared viewport pixels must equal the corresponding shifted pixels.
The layout cache remains bounded at 192 rows.
The test uses a temporary user database.

The resource check covers every bundled file listed in the manifest.
Literata and the explicit Noto fallbacks cover all 314 distinct corpus characters.
The check also rejects leftover import markup.
The full corpus contains 102,270 nonempty verses.
The database opens read-only. A separate database stores personal settings.

## Platform checks

Windows and Linux builds did not run in this local macOS session.
The repository includes a CI workflow for both platforms and macOS.
The workflow requires repository publication before it can run.

Manual checks remain for Windows and Linux native scrolling and font registration.
Checks also remain for Intel Macs, macOS 11, assistive technology, multiple displays, and real overnight wake behavior.
The date model test simulates a clock change. It does not replace an overnight session.

## Remaining scope

Calendar rules and references are computed offline. Later annual Antiochian instructions can change particular assignments.
The 2026 Sunday chart is the completed independent comparison. Other complete years have not received this comparison.
Old calendar mode applies Greek rules to Julian fixed dates. It does not represent a separate approved Antiochian jurisdiction.

Each publisher's complete available edition is bundled.
Some books exist in only one language. Empty publisher placeholders are excluded.
MT/LXX verse alignment remains curated rather than complete.
The reader shows unavailable alignments and can open each edition directly through the Bible browser.

The renderer fits whole paragraphs with native shaping, Knuth–Plass demerits, and Liang hyphenation.
Paragraphs currently follow verse boundaries. Semantic paragraph grouping remains future work.
The speech interface still shows a pronunciation preview. It does not synthesize audio.
