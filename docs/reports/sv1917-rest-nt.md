# Övriga NT: löpande bildgranskning

**Uppdaterad 2026-10-26. Aktiv gren: `codex/sv1917-rest-nt-verification`, [PR #68](https://github.com/jsjoberg/perikop/pull/68), med evangeliegrenen som tillfällig bas.** Användaren har prioriterat resten av NT före återstående GT och godkänt fortsatt arbete bok för bok utan ny uppmaning.

Målet är full visuell jämförelse av Apostlagärningarna, breven och Uppenbarelseboken: **23 böcker, 171 kapitel och 4 173 versposter**. Detta är en separat etapp från [evangeliernas PR #65](https://github.com/jsjoberg/perikop/pull/65). Utökade kanon är färdig i sammanslagna PR #64; dess textgranskningsmål för 0.2.1 utvidgas inte.

Samma granskningskrav som i [huvudliggaren](sv1917-verification.md) gäller: hela kapitel, alla ord och skiljetecken samt versgränser och fortsättningar mellan spalter/sidor. Säkra fel rättas med versbundna ersättningar och regressioner. Möjliga tryckvarianter sparas med stabila V-ID, fulla verser och läsbara originalutsnitt. Codex tolkar kompletterande foton när de finns; användaren behöver inte transkribera eller diagnostisera. Vanliga kommaskillnader, citatglyfer och uttryckligen antecknade versalskillnader harmoniseras inte automatiskt.

## Aktuell status

| Del | Helt bildlästa kapitel | Verifierade kapitel | Bildlästa versposter | Verifierade versposter |
| --- | ---: | ---: | ---: | ---: |
| Apostlagärningarna | 28/28 | 21/28 | 1 002/1 002 | 723/1 002 |
| Breven och Uppenbarelseboken | 0/143 | 0/143 | 0/3 171 | 0/3 171 |
| **Övriga NT totalt** | **28/171** | **21/171** | **1 002/4 173 (24,0 %)** | **723/4 173 (17,3 %)** |

**Hela Apostlagärningarna är bildläst:** 28 kapitel, 1 002 versposter och 24 238 ord på NT s. 185–238. **Verifierat:** kapitel 1, 3, 6–12, 14, 16, 18–24 och 26–28, tillsammans 723 poster och 17 620 ord. Sju helt lästa kapitel med 279 poster väntar på tio frågor. Hela NT-etappen omfattar 99 199 ord; 24,4 % är bildlästa och 17,8 % verifierade. Detta anger texttäckning, inte tidsåtgång eller förväntad felkvot.

**R198–R216:** nitton rättade versposter. Bland annat ort → ord i 2:41, vad → var i 2:47, nå → då i 3:21, borttaget främmande i i 4:7, är → än i 4:19, uträcker → utsträcker i 4:30, du → de i 4:32, visst → visste i 5:24, den → dem i 5:25 och 9:41, han → hans i 7:4, och → om i 7:13, är äter → år åter i 7:30, och → ock i 7:38, gjord → gjort i 9:39, två citatavslut, semikolon i 10:32 och meningsinledande versal i 12:8. [Erratatabellen](sv1917-errata.md) länkar varje rättelse till trycksidan.

**R217–R248:** ytterligare 32 rättade versposter i Apg. 13–28. Bland annat han → hans i 13:25, mena → mina i 13:38, men → man i 13:42, Denne → Denna i 16:17, tid → dit i 16:20, Men, → Medan i 17:16, sade → hade i 18:25, man → män i 19:25, land vägen → landvägen i 20:13, fram → from i 22:12, låt → lät i 22:30, har → här i 25:24, satt → sätt i 26:24, mån → män i 27:10 och bar → har i 28:28 samt citatmarkörer och skiljetecken. R223 rättar citatavslutet i 17:7 utan att avgöra versgränsfrågan; R241 tar bort ett främmande semikolon i 25:26 utan att ta bort har. **Totalt 51 rättade versposter i Apostlagärningarna.**

**Utan nya rättelser:** kapitel 1, 6, 8, 11, 14, 23 och 24. Apg. 8:37, 15:34, 24:7 och 28:29 har bara nothänvisningar i trycket; ingen saknad löptext har skapats. Den ovanliga versgränsen mellan 2:46 och 2:47 samt gränsen 25:25–26 bekräftas av tryckbilderna. Originalupplösning bekräftar Myrra i 27:5 och kvar i 24:27 utan fotofrågor. Tryckets uppenbara be i 16:38 och enot i 18:13 kopieras inte till korpusens de och emot. Kapiteltextens och TIFF-bildernas exakta SHA-256 finns i [JSON-liggaren](sv1917-verification.json).

## Sparat till senare boktillgång

Tio frågor **V123–V132** i [NT-fotoöversikten](sv1917-nt-questions/README.md): Apg. 2:2, 4:1, 4:21, 5:1, 5:13, 13:16, 13:45, 15:24, 17:6–7 och 25:26. Orden och versgränserna behålls tills ett annat tryck har bedömts; kapitel 2, 4, 5, 13, 15, 17 och 25 står som `needs-review` och deras 279 poster räknas inte som verifierade. Alla frågor har fulla aktuella verser, fulla sidlänkar, direkt originalutsnitt och TIFF-/utsnittshashar. V131 gäller gränsen mellan 17:6 och 17:7 och behöver båda verserna med synliga nummer. Detta är inte tio fastställda fel. **Ingen omedelbar fotoinsats behövs.**

## Återuppta här

**Nästa hela bok: Romarbrevet, från 1:1 på NT s. 239.** Inga Romarbrevskapitel har ännu full bildtäckning eller kapitelstatus. Därefter återstående brev och Uppenbarelseboken i bokordning. Även redan riktat ordkontrollerade böcker behöver full jämförelse. Olästa kapitel räknas inte som granskade.

**Den separata NT-PR:n ska vara öppet utkast tills alla dess 171 kapitel är bildlästa, införda rättelser klarar kontrollerna och alla dess sparade läsningsfrågor är avgjorda.** PR #65 behåller sina egna evangeliefrågor och sin befintliga avgränsning. Den nya grenen är tills vidare baserad på evangeliegrenen; när #65 har sammanslagits ska denna PR riktas mot main och vid behov ombaseras.

## Arbetslogg

- **2026-10-26, NT1:** Hela Apg. 1–12 bildjämförda, inklusive alla spalt-/sidfortsättningar. Nitton säkra rättelser R198–R216 bekräftade i originalupplösning och införda. Nio kapitel med 312 poster verifierade; tre med 126 poster väntar på fem sparade frågor V123–V127. Inga verskoordinater eller mappningar ändrade. Tidigare AP-/GT-/evangelieposter behållna. Nästa hela kapitel Apg. 13. Liggare, kandidataudit, kärntest med nitton nya regressioner, sex OSB-metadatatest, 122 resurshashar, fonttäckning och blankstegskontroll godkända. Exakt databasjämförelse bekräftar endast nitton avsedda versändringar, oförändrade övriga texter/koordinater och tidigare liggarposter. R204 återställer uppslagskopplingen utsträcker → utsträcka; övriga lexikondata oförändrade. Alla 119 sparade evangelie-/NT-utsnitt har giltiga hashvärden.

- **2026-10-26, NT2:** Hela Apg. 13–28, 564 poster och 14 033 ord, bildjämförda på NT s. 208–238. R217–R248 rättar 32 versposter. Tolv kapitel med 411 poster och 10 227 ord tillförda som verifierade; fyra kapitel med 153 poster väntar på fem frågor V128–V132 i NT-fotobunt 2. Hela Apostlagärningarna bildläst; 21/28 kapitel och 723/1 002 poster verifierade. Inga ord eller mappningar ändrade för versgränsfrågan 17:6–7. Nästa bok Romarbrevet. Liggare, kandidataudit, kärntest med 32 nya regressioner, sex OSB-metadatatest, 122 resurshashar, fonttäckning och blankstegs-/TSV-kontroll godkända. Exakt blockjämförelse bekräftar endast 32 avsedda versändringar, oförändrade övriga texter/koordinater, mappningar, lexikondata och tidigare liggarposter. Hela NT-etappens diff mot evangeliegrenen omfattar exakt 51 avsedda Apg.-verser; inga andra verser ändras. Samtliga 124 sparade evangelie-/NT-utsnitt matchar angivna hashvärden.
