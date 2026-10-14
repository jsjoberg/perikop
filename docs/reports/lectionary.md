> Report as of 2026-10-14 (d20a5e4). Not kept up to date; check the code before relying on it.

# Reading calculation

The calendar offers two reading families:

- Greek uses the shared Greek lectionary and the annual assignments of the Greek Orthodox Archdiocese of America (GOA).
- Slavic uses the Russian and OCA lectionary. It has no annual assignments.

The Greek family also includes the Antiochian Archdiocese of North America's order.
When the readings differ, the day page shows both readings with their church names and an explanation.
Shared readings appear once. Comparisons include every segment and its reference edition.
The explanations identify verse-range differences and different day titles. They do not claim a historical reason for every difference.
Existing Antiochian settings select the Greek family.

The variant model supports additional jurisdiction readings and omissions.
The current tables do not supply separate Serbian or Georgian reading orders.
The application labels the Slavic source as Russian/OCA. It does not imply complete coverage of every church in this family.
Additional jurisdiction variants need independent source tables before the application can show them.

The database rows have a tradition tag: common, greek, slavic, or antiochian.
A tradition's row replaces the common row with the same slot. Antiochian rows also replace Greek rows.

The Gregoriansk / reviderad juliansk option uses Revised Julian fixed dates.
It matches current Gregorian dates through February 2800. Later fixed dates use the Revised Julian rule.
Civil dates always remain Gregorian.
Juliansk applies the selected reading order to Julian fixed dates.

C++ calculates Orthodox Pascha with the Julian computus.
It converts the result to the civil Gregorian date.
Fixed dates and the Pascha distance remain separate.
Julian conversion calculates the century-dependent difference. It also preserves Julian leap-day labels.

The engine calculates the following reading positions:

- The Triodion, Lent, Holy Week, Pascha, and Pentecost cycles.
- The continuous Epistle cycle.
- The autumn Lukan jump after the Sunday following the Elevation.
- The reserved autumn Sunday windows and Apostle feast replacements.
- The winter interpolation of unused Sunday readings before the Triodion.
- Saturdays and Sundays around Nativity, Theophany, and the Elevation.
- Fixed feasts and All Saints of Antioch.

The Greek and Antiochian orders share these Gospel rules.
The Slavic order uses the Slavic rules instead:

- After the Lukan jump, the Sunday Gospels follow the ordinary sequence. The eleventh Sunday of Luke reads the Forefathers Gospel.
- Sunday Gospels that the feasts and the Lukan jump displace are read on the Sundays after Theophany.
- The Synaxis of the Unmercenaries and the New Martyrs of Russia are floating feasts.

The abbreviated Slavic calculation selects one Epistle and one Gospel with Orthocal's rule.
A floating feast's pair comes first. On a weekday, a fixed feast of rank 3 or higher comes before the ordinary pair.
On a Sunday, the ordinary pair comes first. Clean Week and Holy Monday to Wednesday have no fixed feast readings.
Unlike Orthocal, a moveable day of higher rank keeps its own pair. Ascension keeps its readings when it falls on a saint's day.
Holy Week days without a complete pair show their Gospels with the prophecies.

The database supplies recurring pericope references and feast metadata.
It does not store a finite list of computed years.
The application calendar exposes civil dates from January 1, 2019, through December 31, 9999.
Day buttons, month and year navigation, and selected-date validation enforce that lower bound.
The engine retains proleptic arithmetic for earlier years only for internal calculation and cycle tests.
The tests check every day of seven distant years and a complete 532-year Julian Paschal cycle.
Published annual assignments override recurring positions only for their stated dates, in new calendar mode.
Bundled Orthocal annual assignments cover 2019–2026 for Antiochian and 2019–2027 for Greek.
Assignments before 2019 are excluded from both jurisdictions.
After the last published year, these dates use the calculated reading.
Later annual instructions can require additional exceptions.

The rules and tables derive from [Orthocal](https://github.com/brianglass/orthocal-python), revision `eba3af4d6552f43ea09079e19b3f6147619b6dfb`.
The import excludes third-party Scripture wording and composite wording.
Composite readings retain their published citations, so the day page shows that they apply.
They have no read button because their adapted text is not a contiguous Scripture passage.
The upstream MIT license remains in the package.

The official Antiochian Sunday charts for [2019](https://antiochianprodsa.blob.core.windows.net/liturgicalinstructions/Liturgical%20Chart%20for%202019%20English.pdf), [2020](https://antiochianprodsa.blob.core.windows.net/liturgicalinstructions/Liturgical%20Chart%20for%202020%20English.pdf), [2021](https://antiochianprodsa.blob.core.windows.net/liturgicalinstructions/Liturgical%20Chart%20for%202021%20English.pdf), [2022](https://antiochianprodsa.blob.core.windows.net/liturgicalinstructions/Liturgical%20Chart%20for%202022%20English.pdf), [2023](https://antiochianprodsa.blob.core.windows.net/liturgicalinstructions/Liturgical%20Chart%20for%202023%20English.pdf), [2024](https://antiochianprodsa.blob.core.windows.net/liturgicalinstructions/Liturgical%20Chart%20for%202024%20English.pdf), [2025](https://antiochianprodsa.blob.core.windows.net/liturgicalinstructions/Liturgical%20Chart%20for%202025%20English.pdf), [2026](https://antiochianprodsa.blob.core.windows.net/liturgicalinstructions/Liturgical%20Chart%20for%202026%20English.pdf) supply all 417 Sunday Divine Liturgy Epistle/Gospel pairs.
`resources/lectionary/antiochian.json` records each pair for its exact civil date and year, with the chart URL on every reading.
The manifest pins the original PDF hashes and records the scope. No Bible wording is imported.
The annual `tests/antiochian-YYYY.tsv` fixtures for 2019–2026 check the exact published segments in both abbreviated and full service modes.
The chart pair replaces the main calculated Liturgy pair for that Sunday.
Additional saint readings and other services remain calculated from Orthocal; the chart does not establish their omission.
The runtime corpus preserves the chart URL on each assigned reading, separately from its Bible edition.
The discontinuous readings, chapter crossings, and Matthew 25:14–30 with the added Luke 8:8 ending on February 11, 2024, remain exact.

These annual charts apply only in New Calendar mode and only to their stated dates.
The former five recurring overrides derived from 2026 are removed.
For example, June 22, 2025, uses Romans 2:10–16; June 14, 2026, uses Acts 11:19–30 for All Saints of Antioch.
A published annual exception is never projected into another year or Old Calendar mode.
No official Antiochian 2027 Sunday chart was located during the October 14, 2026, audit.
2027 and later use the calculated references pending publication and independent comparison.
Weekdays, Matins, Vespers, and other services still derive from Orthocal; the Sunday charts do not verify those readings.

`tests/oca-2026.tsv` contains oca.org's daily readings for 2026, as Orthocal collected them.
The test compares the Slavic order with every day.
The Slavic Epistle and Gospel are one of oca.org's pairs on 330 days, and its first pair on 293 days.
The test names four known exceptions:

- February 24 and 27: Orthocal reads no fixed feast readings in Clean Week. oca.org reads the feasts.
- October 31: oca.org's saint of the day is not in Orthocal's tables.
- November 8: Orthocal reads the Unmercenaries on the Sunday after November 1. oca.org reads the Archangels.

The Archdiocese's daily references for October 5, 2026, are Philippians 1:1–7 and Luke 6:24–30.
October 6 uses 1 Corinthians 4:9–16 and John 20:19–31 for Apostle Thomas.
Both dates match the engine.

Discontinuous readings retain their segment order.
The reader marks only those segments. Speech playback also excludes omitted verses.
The segment selector opens each part, including parts in another book.

The daily list shows all service readings that match the selected date and reading family.
It includes Vespers, Matins, the Hours, the Great Blessing of Waters, and other services when applicable.
Saints' Epistles and Gospels appear beside the ordinary daily readings, with their occasion names.
The eleven Sunday Matins Gospels follow their Paschal cycle. Lenten feast Vespers move to the preceding day where the rules require it.
The separate abbreviated calculation retains one Epistle/Gospel pair for comparison with published daily charts.
The original specification's morning and evening Psalm rules remain unspecified.

## Reading calendar

The day heading identifies the day and week in the reading cycle, without adding fixed feast headings.
The month calendar marks dates with readings for Orthros, Vespers, and other services, such as the Hours.
Date tooltips list the assigned services and reading counts. They include the Antiochian alternatives shown on the day page.
The markers follow the selected reading family and fixed-calendar style.

Perikop does not provide fasting guidance, feast markers, or commemoration lists.
The calculation retains feast dates and ranks to select the correct Scripture readings.
Occasion names identify the readings to which they belong.

## Serbian Old Calendar comparison

Independent Serbian Church publications confirm these civil dates in 2026:

- Nativity: January 7, from the [Diocese of Timok](https://eparhija-timocka.org/bozic-u-eparhiji-timockoj-2026/).
- Theophany: January 19, from the [Serbian Patriarchate](https://spc.rs/sr/news/15565.episkop-jerotej-krstenjem-postajemo-sinovi-carstva-bozijeg.html).
- Pascha: April 12, from the [Diocese of Bačka](https://eparhijabacka.info/2026/04/12/praznik-hristovog-vaskrsenja-u-sabornom-hramu-u-novom-sadu-3/).
- Elevation: September 27, from the [Serbian Patriarchate](https://spc.rs/sr/news/17927.paradoks-krsta-%E2%80%93-stradanje-postaje-izvor-radosti-zivota.html).

The Eastern American Diocese publishes [Matthew 2:13–23 for January 11](https://www.easterndiocese.org/files/2026/FrRodney/The-Sunday-affter-the-Nativity--January-11-2026.docx.pdf).
It also publishes [John 1:1–17 for Pascha](https://www.easterndiocese.org/files/2026/FrRodney/Pascha-April-12-2026....pdf).
Both Gospel ranges match the Slavic and Greek orders in Old Calendar mode.
These checks cover shared dates and readings. Serbian saints are not in the tables.

The Revised Julian calendar uses the 900-year leap rule: century years must leave remainder 200 or 600.
[Stellarium's calendar implementation](https://github.com/Stellarium/stellarium/blob/master/plugins/Calendars/src/RevisedJulianCalendar.cpp) provides an independent technical reference.
No Stellarium source code is included in the application.
