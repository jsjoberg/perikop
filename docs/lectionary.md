# Reading calculation

The calendar offers three reading orders:

- The Antiochian order is the Greek order with the own readings of the Antiochian Orthodox Christian Archdiocese of North America.
- The Greek order is the shared Greek lectionary. It uses the annual assignments of the Greek Orthodox Archdiocese of America.
- The Slavic order is the Russian and OCA lectionary. It has no annual assignments.

The database rows have a tradition tag: common, greek, slavic, or antiochian.
A tradition's row replaces the common row with the same slot. Antiochian rows also replace Greek rows.

The Nya kalendern option uses Revised Julian fixed dates.
It matches current Gregorian dates through February 2800. Later fixed dates use the Revised Julian rule.
Civil dates always remain Gregorian.
Gamla kalendern applies the selected reading order to Julian fixed dates.

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

The Slavic order selects one Epistle and one Gospel with Orthocal's rule.
A floating feast's pair comes first. On a weekday, a fixed feast of rank 3 or higher comes before the ordinary pair.
On a Sunday, the ordinary pair comes first. Clean Week and Holy Monday to Wednesday have no fixed feast readings.
Unlike Orthocal, a moveable day of higher rank keeps its own pair. Ascension keeps its readings when it falls on a saint's day.
Holy Week days without a complete pair show their Gospels with the prophecies.

The database supplies recurring pericope references and feast metadata.
It does not store a finite list of computed years.
The supported civil date range is 1–9999. Earlier dates use proleptic rules, rather than historical adoption dates.
The tests check every day of seven distant years and a complete 532-year Julian Paschal cycle.
Published annual assignments override recurring positions only for their stated dates, in new calendar mode.
The Antiochian assignments cover 2019–2026. The Greek assignments cover 2011–2027.
After the last published year, these dates use the calculated reading.
Later annual instructions can require additional exceptions.

The rules and tables derive from [Orthocal](https://github.com/brianglass/orthocal-python), revision `eba3af4d6552f43ea09079e19b3f6147619b6dfb`.
The import excludes liturgical composites and third-party Scripture wording.
The upstream MIT license remains in the package.

The [official 2026 Sunday chart](https://antiochianprodsa.blob.core.windows.net/liturgicalinstructions/Liturgical%20Chart%20for%202026%20English.pdf) supplies the comparison references.
All 52 Sundays match, including when every annual override is removed.
The comparison checks every segment, including omitted verses and chapter crossings.
The Antiochian readings are in `resources/lectionary/antiochian.json`, in Orthocal's record format.
They include All Saints of Antioch, the Samaritan Sunday Epistle, and the Sunday of Orthodoxy Epistle.
The Luke and Evangelist Epistle and the twenty-fifth Sunday Epistle also retain the chart's exact ranges.
The 2026 chart is the only source for these readings. They apply in every year and in both calendar modes.

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

The daily list contains the prescribed Epistle and Gospel, or weekday Lenten prophecy readings.
It does not claim to reproduce every liturgical service or composed Vespers reading.
The original specification's morning and evening Psalm rules remain unspecified.

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

The New calendar uses the 900-year leap rule: century years must leave remainder 200 or 600.
[Stellarium's calendar implementation](https://github.com/Stellarium/stellarium/blob/master/plugins/Calendars/src/RevisedJulianCalendar.cpp) provides an independent technical reference.
No Stellarium source code is included in the application.
