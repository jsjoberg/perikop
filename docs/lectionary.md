# Antiochian reading calculation

The calendar uses the Greek tradition of the Antiochian Orthodox Christian Archdiocese of North America.
The Nya kalendern option uses Revised Julian fixed dates.
It matches current Gregorian dates through February 2800. Later fixed dates use the Revised Julian rule.
Civil dates always remain Gregorian.
The Old calendar applies the same reading rules to Julian fixed dates.
Gamla kalendern applies the Greek reading tradition to Julian fixed dates.
It remains a comparison mode, rather than an official North American or Serbian calendar.

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

The database supplies recurring pericope references and feast metadata.
It does not store a finite list of computed years.
The supported civil date range is 1–9999. Earlier dates use proleptic rules, rather than historical adoption dates.
The tests check every day of seven distant years and a complete 532-year Julian Paschal cycle.
Published Antiochian annual assignments override recurring positions only for their stated dates.
Later annual instructions can require additional exceptions.

The rules and tables derive from [Orthocal](https://github.com/brianglass/orthocal-python), revision `5bdf0a5e1cad406388d7860ec74ed506a7a19197`.
The import excludes Slavic-specific records and third-party Scripture wording.
The upstream MIT license remains in the package.

The [official 2026 Sunday chart](https://antiochianprodsa.blob.core.windows.net/liturgicalinstructions/Liturgical%20Chart%20for%202026%20English.pdf) supplies the comparison references.
All 52 Sundays match, including when every annual override is removed.
The comparison checks every segment, including omitted verses and chapter crossings.
The recurring Antiochian variants are explicit in the corpus builder.
They include All Saints of Antioch, the Samaritan Sunday Epistle, and the Sunday of Orthodoxy Epistle.
The Luke and Evangelist Epistle and the twenty-fifth Sunday Epistle also retain the chart's exact ranges.

The Archdiocese's daily references for October 5, 2026, are Philippians 1:1–7 and Luke 6:24–30.
October 6 uses 1 Corinthians 4:9–16 and John 20:19–31 for Apostle Thomas.
Both dates match the engine.

Discontinuous readings retain their segment order.
The reader marks only those segments. The speech preview also excludes omitted verses.
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
Both Gospel ranges match the calculated Old Calendar mode.
These checks cover shared dates and readings. Serbian saints and Slavic reading precedence remain a separate jurisdictional feature.

The New calendar uses the 900-year leap rule: century years must leave remainder 200 or 600.
[Stellarium's calendar implementation](https://github.com/Stellarium/stellarium/blob/master/plugins/Calendars/src/RevisedJulianCalendar.cpp) provides an independent technical reference.
No Stellarium source code is included in the application.
