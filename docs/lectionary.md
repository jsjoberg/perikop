# Antiochian reading calculation

The calendar uses the Greek tradition of the Antiochian Orthodox Christian Archdiocese of North America.
The New calendar uses Gregorian fixed dates, as the source tables do.
The Old calendar applies the same reading rules to Julian fixed dates.
Old calendar mode is a comparison mode. It is not the official North American calendar.

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
Published Antiochian annual assignments override recurring positions only for their stated dates.
Later annual instructions can require additional exceptions.

The rules and tables derive from [Orthocal](https://github.com/brianglass/orthocal-python), revision `5bdf0a5e1cad406388d7860ec74ed506a7a19197`.
The import excludes Slavic-specific records and third-party Scripture wording.
The upstream MIT license remains in the package.

The [official 2026 Sunday chart](https://antiochianprodsa.blob.core.windows.net/liturgicalinstructions/Liturgical%20Chart%20for%202026%20English.pdf) supplies the comparison references.
All 52 Sundays match. The comparison checks every segment, including omitted verses and chapter crossings.
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
