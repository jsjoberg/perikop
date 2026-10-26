# Övriga NT: löpande bildgranskning

**Uppdaterad 2026-10-26. Aktiv gren: `codex/sv1917-rest-nt-verification`, [PR #68](https://github.com/jsjoberg/perikop/pull/68), med evangeliegrenen som tillfällig bas.** Användaren har prioriterat resten av NT före återstående GT och godkänt fortsatt arbete bok för bok utan ny uppmaning.

Målet är full visuell jämförelse av Apostlagärningarna, breven och Uppenbarelseboken: **23 böcker, 171 kapitel och 4 173 versposter**. Detta är en separat etapp från [evangeliernas PR #65](https://github.com/jsjoberg/perikop/pull/65). Utökade kanon är färdig i sammanslagna PR #64; dess textgranskningsmål för 0.2.1 utvidgas inte.

Samma granskningskrav som i [huvudliggaren](sv1917-verification.md) gäller: hela kapitel, alla ord och skiljetecken samt versgränser och fortsättningar mellan spalter/sidor. Säkra fel rättas med versbundna ersättningar och regressioner. Möjliga tryckvarianter sparas med stabila V-ID, fulla verser och läsbara originalutsnitt. Codex tolkar kompletterande foton när de finns; användaren behöver inte transkribera eller diagnostisera. Vanliga kommaskillnader, citatglyfer och uttryckligen antecknade versalskillnader harmoniseras inte automatiskt.

## Aktuell status

| Del | Helt bildlästa kapitel | Verifierade kapitel | Bildlästa versposter | Verifierade versposter |
| --- | ---: | ---: | ---: | ---: |
| Apostlagärningarna | 28/28 | 21/28 | 1 002/1 002 | 723/1 002 |
| Romarbrevet | 16/16 | 12/16 | 432/432 | 319/432 |
| Återstående brev och Uppenbarelseboken | 0/127 | 0/127 | 0/2 739 | 0/2 739 |
| **Övriga NT totalt** | **44/171** | **33/171** | **1 434/4 173 (34,4 %)** | **1 042/4 173 (25,0 %)** |

**Hela Apostlagärningarna är bildläst:** 28 kapitel, 1 002 versposter och 24 238 ord på NT s. 185–238. **Verifierat:** kapitel 1, 3, 6–12, 14, 16, 18–24 och 26–28, tillsammans 723 poster och 17 620 ord. Sju helt lästa kapitel med 279 poster väntar på tio frågor. Hela NT-etappen omfattar 99 199 ord; efter Romarbrevet är 34 015 ord (34,3 %) bildlästa och 25 042 (25,2 %) verifierade. Detta anger texttäckning, inte tidsåtgång eller förväntad felkvot.

**R198–R216:** nitton rättade versposter. Bland annat ort → ord i 2:41, vad → var i 2:47, nå → då i 3:21, borttaget främmande i i 4:7, är → än i 4:19, uträcker → utsträcker i 4:30, du → de i 4:32, visst → visste i 5:24, den → dem i 5:25 och 9:41, han → hans i 7:4, och → om i 7:13, är äter → år åter i 7:30, och → ock i 7:38, gjord → gjort i 9:39, två citatavslut, semikolon i 10:32 och meningsinledande versal i 12:8. [Erratatabellen](sv1917-errata.md) länkar varje rättelse till trycksidan.

**R217–R248:** ytterligare 32 rättade versposter i Apg. 13–28. Bland annat han → hans i 13:25, mena → mina i 13:38, men → man i 13:42, Denne → Denna i 16:17, tid → dit i 16:20, Men, → Medan i 17:16, sade → hade i 18:25, man → män i 19:25, land vägen → landvägen i 20:13, fram → from i 22:12, låt → lät i 22:30, har → här i 25:24, satt → sätt i 26:24, mån → män i 27:10 och bar → har i 28:28 samt citatmarkörer och skiljetecken. R223 rättar citatavslutet i 17:7 utan att avgöra versgränsfrågan; R241 tar bort ett främmande semikolon i 25:26 utan att ta bort har. **Totalt 51 rättade versposter i Apostlagärningarna.**

**Utan nya rättelser:** kapitel 1, 6, 8, 11, 14, 23 och 24. Apg. 8:37, 15:34, 24:7 och 28:29 har bara nothänvisningar i trycket; ingen saknad löptext har skapats. Den ovanliga versgränsen mellan 2:46 och 2:47 samt gränsen 25:25–26 bekräftas av tryckbilderna. Originalupplösning bekräftar Myrra i 27:5 och kvar i 24:27 utan fotofrågor. Tryckets uppenbara be i 16:38 och enot i 18:13 kopieras inte till korpusens de och emot. Kapiteltextens och TIFF-bildernas exakta SHA-256 finns i [JSON-liggaren](sv1917-verification.json).

**Hela Romarbrevet är bildläst:** 16 kapitel, 432 poster och 9 777 ord på NT s. 239–262. **R249–R252:** sig själv → sig själva i 2:14, i → I i 8:13, och → ock i 10:15 samt hälsar → hälsa i 16:23. Kapitel 1, 2, 4, 5, 7, 8, 10–15 är verifierade: 319 poster och 7 422 ord. Fyra kapitel med 113 poster väntar på V133–V136. Originalupplösningen bekräftar båda de i 8:5 och till i 12:7 utan foto; skadade rader i 11:33/36 är läsbara och överensstämmer. Tryckets hava i 11:30 kopieras inte till korpusens grammatiska haven. Vers 16:24 finns endast som nothänvisning i trycket. Vanliga dokumenterade skiljeteckens- och ordmellanrumsskillnader behålls.

## Sparat till senare boktillgång

Fjorton frågor **V123–V136** i [NT-fotoöversikten](sv1917-nt-questions/README.md): tio tidigare frågor i Apostlagärningarna och fyra i Romarbrevet 3:7–8, 6:20–21, 9:15 och 16:26. Orden och versgränserna behålls tills ett annat tryck har bedömts. Elva helt bildlästa kapitel med 392 poster står som `needs-review`. Alla frågor har fulla aktuella verser, fulla sidlänkar, direkt originalutsnitt och TIFF-/utsnittshashar. V131, V133 och V134 behöver båda berörda verserna med synliga nummer. Detta är inte fjorton fastställda fel. **Ingen omedelbar fotoinsats behövs.**

## Återuppta här

**Nästa hela bok: Första Korinthierbrevet, från 1:1 på NT s. 263.** Därefter återstående brev och Uppenbarelseboken i bokordning. Även redan riktat ordkontrollerade böcker behöver full jämförelse. Olästa kapitel räknas inte som granskade.

**Den separata NT-PR:n ska vara öppet utkast tills alla dess 171 kapitel är bildlästa, införda rättelser klarar kontrollerna och alla dess sparade läsningsfrågor är avgjorda.** PR #65 behåller sina egna evangeliefrågor och sin befintliga avgränsning. Den nya grenen är tills vidare baserad på evangeliegrenen; när #65 har sammanslagits ska denna PR riktas mot main och vid behov ombaseras.

## Arbetslogg

- **2026-10-26, NT1:** Hela Apg. 1–12 bildjämförda, inklusive alla spalt-/sidfortsättningar. Nitton säkra rättelser R198–R216 bekräftade i originalupplösning och införda. Nio kapitel med 312 poster verifierade; tre med 126 poster väntar på fem sparade frågor V123–V127. Inga verskoordinater eller mappningar ändrade. Tidigare AP-/GT-/evangelieposter behållna. Nästa hela kapitel Apg. 13. Liggare, kandidataudit, kärntest med nitton nya regressioner, sex OSB-metadatatest, 122 resurshashar, fonttäckning och blankstegskontroll godkända. Exakt databasjämförelse bekräftar endast nitton avsedda versändringar, oförändrade övriga texter/koordinater och tidigare liggarposter. R204 återställer uppslagskopplingen utsträcker → utsträcka; övriga lexikondata oförändrade. Alla 119 sparade evangelie-/NT-utsnitt har giltiga hashvärden.

- **2026-10-26, NT2:** Hela Apg. 13–28, 564 poster och 14 033 ord, bildjämförda på NT s. 208–238. R217–R248 rättar 32 versposter. Tolv kapitel med 411 poster och 10 227 ord tillförda som verifierade; fyra kapitel med 153 poster väntar på fem frågor V128–V132 i NT-fotobunt 2. Hela Apostlagärningarna bildläst; 21/28 kapitel och 723/1 002 poster verifierade. Inga ord eller mappningar ändrade för versgränsfrågan 17:6–7. Nästa bok Romarbrevet. Liggare, kandidataudit, kärntest med 32 nya regressioner, sex OSB-metadatatest, 122 resurshashar, fonttäckning och blankstegs-/TSV-kontroll godkända. Exakt blockjämförelse bekräftar endast 32 avsedda versändringar, oförändrade övriga texter/koordinater, mappningar, lexikondata och tidigare liggarposter. Hela NT-etappens diff mot evangeliegrenen omfattar exakt 51 avsedda Apg.-verser; inga andra verser ändras. Samtliga 124 sparade evangelie-/NT-utsnitt matchar angivna hashvärden.

- **2026-10-26, NT3:** Hela Romarbrevet, 16 kapitel, 432 poster och 9 777 ord, bildjämfört på NT s. 239–262. Fyra säkra rättelser R249–R252 införda. Tolv kapitel med 319 poster och 7 422 ord verifierade; fyra kapitel med 113 poster väntar på V133–V136 i NT-fotobunt 3. Fem originalutsnitt och fulla verser sparade. Förstoringen löser de i 8:5 och till i 12:7 utan foto. NT-etappen 1 434/4 173 poster bildlästa (34,4 %) och 1 042 verifierade (25,0 %). Nästa bok Första Korinthierbrevet på NT s. 263. Liggare, kandidataudit, kärntest med fyra nya regressioner, sex OSB-metadatatest, 122 resurshashar, fonttäckning och blankstegs-/TSV-kontroll godkända. Exakt blockjämförelse bekräftar endast fyra avsedda Rom.-versändringar; tidigare liggarposter, övriga texter/koordinater, mappningar och lexikondata oförändrade. Hela NT-etappen omfattar exakt 55 avsedda Apg.-/Rom.-versändringar mot evangeliebasen. Samtliga 129 sparade evangelie-/NT-utsnitt matchar sina SHA-256.
