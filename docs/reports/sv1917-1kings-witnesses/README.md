# Bildvittnen för Första Konungaboken

Bevarade originalfiler till [bokrapporten](../sv1917-errata-1kings.md).
[Manifestet](manifest.json) anger filernas SHA-256 och exakta hämtningsadresser.

- `efs1927/0461.tif`–`0506.tif`: 46 Runeberg-bilder, EFS 1927 s. 461–506. Alla 817 versposter i 22 kapitel har lästs visuellt, inklusive fortsättningar över spalt- och sidgränser. Angränsande böcker på delade sidor räknas inte.
- `efs1926/n435.jpg`–`n476.jpg`: 42 faktiska bilder ur EFS Stockholm 1926, s. 424–465, Internet Archive `bibelnellerdenhe0000unse`. Föreslagna rättelser och editionsskillnader jämfördes med dessa bilder. IA:s `n`-nummer är tryckt sidnummer + 11; de är inte Runebergs sidnummer. OCR användes bara för att hitta sidor.
- `ia-orig0462.jp2`, `ia-orig0463.jp2`, `ia-orig0465.jp2`: tre ursprungliga kamerafiler ur samma IA-exemplar, hämtade separat ur originalarkivet. Även den motstående sidans marginal kan innehålla läsbar text.
- `orig462-detail.png`: original0462 roterad +90°, utsnitt `[2900,4550,3660,4770]`, förstorat 3×. `orig463-detail.png`: original0463 roterad −90°, utsnitt `[0,4550,340,4780]`, förstorat 4×. De visar varför kommat efter **synder** i 14:22 fortfarande inte kan avgöras: bokryggen döljer platsen även i kamerafilerna. Kapitel 14 har status `needs-review`.
- `orig465-detail.png`: original0465 roterad −90°, utsnitt `[0,3930,340,4250]`, förstorat 4×. Kommat efter **gjorde** i 15:31 är synligt på den motstående sidan och överensstämmer med 1927. Frågan är löst och rättelsen införd.

Utsnittens koordinater gäller den roterade bilden, före förstoring. Inga tecken har ritats till eller rekonstruerats. De 94 filernas bytehashar och kapiteltextens hash kontrolleras med `tests/swedish_1kings_test.py`. Testet kan upptäcka ändrat underlag men intygar inte den visuella tolkningen. Den separata mänskliga kontrollen är inte utförd; alla nya kontrollrutor är omarkerade.
