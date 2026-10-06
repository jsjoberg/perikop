# Validation record

Local check date: October 6, 2026.
The release build used Apple Clang 17 on macOS 26.6.2, on Apple Silicon.
The application targets macOS 11.0. wxWidgets 3.3.3 and SQLite 3.53.4 link statically.

The core suite passed 7,099 checks for storage, date calculations, reading ranges, and pronunciation.
The suite compared all 52 Sunday reading pairs with the Antiochian 2026 chart.
It also checked the October 5–6 daily references and official Pascha dates for 2026–2030.
Computed Pascha readings passed for 2027–2035.
All 52 Sunday pairs also match after removing every annual assignment.
Every day passes in both modes for years 1, 2036, 2100, 2400, 2800, 5000, and 9999.
The complete 532-year Paschal cycle retains its Julian dates and Sunday weekday.
New Calendar leap tests cover its Revised Julian divergence in 2800 and 2900.
Serbian publications supply four shared feast dates and two exact Gospel ranges.
Other checks cover date stability, Julian century leap labels, discontinuous passages, complete corpus endpoints, alignment, pronunciation, and local settings.

The GUI smoke test passed in all three themes and four parallel modes.
It also passed in a narrow window.
The test checks native paragraph justification, optical punctuation, final-line treatment, source-text recovery, and hyphenation in all three languages.
It checks fractional scrolling and compares images before and after a 17-pixel scroll.
At least 99.5% of the compared viewport pixels must equal the corresponding shifted pixels.
The layout cache remains bounded at 192 rows.
The test uses a temporary user database.

The resource check covers every bundled file listed in the manifest.
Literata and the explicit Noto fallbacks cover all 315 distinct corpus characters.
The check also rejects leftover import markup.
The corpus contains 141,036 nonempty Scripture records across five editions.
Greek Daniel, 317 lettered LXX portions, and English deuterocanonical books are included.
One English publisher record contains two joined verse coordinates.
The resource check asserts each edition's exact record and book counts.
The database opens read-only. A separate database stores personal settings.
Migration, explicit SQLite policy, failed-save rollback, and future-version rejection tests pass.
The GUI smoke test also opens lettered Greek text, joined WEB text, and an English-only book.

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
The Bible browser filters its book list by the chosen edition.
This release does not claim full Matins, Vespers, or composite liturgical service coverage.
MT/LXX verse alignment remains curated rather than complete.
The reader shows unavailable alignments and can open each edition directly through the Bible browser.

The renderer fits whole paragraphs with native shaping, Knuth–Plass demerits, and Liang hyphenation.
Prose flows across verses, using USFM boundaries or labelled WEB editorial boundaries for Swedish and KJV.
Poetry retains verse stanzas. Breaks inside a verse remain a display limitation.
An exhaustive word-boundary oracle confirms 39 globally optimal paragraph fits, including choices that differ from greedy wrapping.
The macOS speech backend generated 59,668 non-silent PCM frames from the installed Alva voice.
The installed application showed the cross and continuous justified prose.
Native playback controls passed Listen, Pause, Resume, and Stop checks using Alva.
Cancelled utterance callbacks cannot overwrite the stopped status.
Windows and Linux speech playback remain unavailable.
