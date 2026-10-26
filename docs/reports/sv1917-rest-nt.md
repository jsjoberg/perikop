# Övriga NT: löpande bildgranskning

**Uppdaterad 2026-10-26. Aktiv gren: `codex/sv1917-rest-nt-verification`, med evangeliegrenen som tillfällig bas.** Användaren har prioriterat resten av NT före återstående GT och godkänt fortsatt arbete bok för bok utan ny uppmaning.

Målet är full visuell jämförelse av Apostlagärningarna, breven och Uppenbarelseboken: **23 böcker, 171 kapitel och 4 173 versposter**. Detta är en separat etapp från [evangeliernas PR #65](https://github.com/jsjoberg/perikop/pull/65). Utökade kanon är färdig i sammanslagna PR #64; dess textgranskningsmål för 0.2.1 utvidgas inte.

Samma granskningskrav som i [huvudliggaren](sv1917-verification.md) gäller: hela kapitel, alla ord och skiljetecken samt versgränser och fortsättningar mellan spalter/sidor. Säkra fel rättas med versbundna ersättningar och regressioner. Möjliga tryckvarianter sparas med stabila V-ID, fulla verser och läsbara originalutsnitt. Codex tolkar kompletterande foton när de finns; användaren behöver inte transkribera eller diagnostisera. Vanliga kommaskillnader, citatglyfer och uttryckligen antecknade versalskillnader harmoniseras inte automatiskt.

## Aktuell status

| Del | Helt bildlästa kapitel | Verifierade kapitel | Bildlästa versposter | Verifierade versposter |
| --- | ---: | ---: | ---: | ---: |
| Apostlagärningarna | 12/28 | 9/28 | 438/1 002 | 312/1 002 |
| Breven och Uppenbarelseboken | 0/143 | 0/143 | 0/3 171 | 0/3 171 |
| **Övriga NT totalt** | **12/171** | **9/171** | **438/4 173 (10,5 %)** | **312/4 173 (7,5 %)** |

**Bildläst:** hela Apg. 1–12, 438 versposter och 10 205 ord på NT s. 185–208. **Verifierat:** kapitel 1, 3, 6–12, tillsammans 312 poster och 7 393 ord. Hela etappen omfattar 99 200 ord; 10,3 % är bildlästa och 7,5 % verifierade. Detta anger texttäckning, inte tidsåtgång eller förväntad felkvot.

**R198–R216:** nitton rättade versposter. Bland annat ort → ord i 2:41, vad → var i 2:47, nå → då i 3:21, borttaget främmande i i 4:7, är → än i 4:19, uträcker → utsträcker i 4:30, du → de i 4:32, visst → visste i 5:24, den → dem i 5:25 och 9:41, han → hans i 7:4, och → om i 7:13, är äter → år åter i 7:30, och → ock i 7:38, gjord → gjort i 9:39, två citatavslut, semikolon i 10:32 och meningsinledande versal i 12:8. [Erratatabellen](sv1917-errata.md) länkar varje rättelse till trycksidan.

**Utan nya rättelser:** kapitel 1, 6, 8 och 11. Apg. 8:37 har bara en nothänvisning i trycket; ingen saknad löptext har skapats. Den ovanliga versgränsen mellan 2:46 och 2:47 bekräftas av tryckbilden. Kapiteltextens och TIFF-bildernas exakta SHA-256 finns i [JSON-liggaren](sv1917-verification.json).

## Sparat till senare boktillgång

Fem frågor **V123–V127** i [NT-fotobunt 1](sv1917-nt-questions/batch-1.md): Apg. 2:2, 4:1, 4:21, 5:1 och 5:13. Orden behålls i korpusen tills ett annat tryck har bedömts; de tre kapitlen står som `needs-review` och deras 126 poster räknas inte som verifierade. Varje fråga har full aktuell vers, full sidlänk, direkt originalutsnitt och TIFF-/utsnittshashar. Detta är inte fem fastställda fel. **Ingen omedelbar fotoinsats behövs.** [Samlad NT-fotoöversikt](sv1917-nt-questions/README.md).

## Återuppta här

**Nästa hela kapitel: Apostlagärningarna 13.** Vers 1–13 har också jämförts på s. 208 men ger ingen kapitelstatus; fortsätt från 13:14 på NT s. 209, med återkontroll av kapitlets början. Därefter Apg. 14–28, Romarbrevet och återstående NT i bokordning. Även redan riktat ordkontrollerade böcker behöver full jämförelse. Olästa kapitel räknas inte som granskade.

**Den separata NT-PR:n ska vara öppet utkast tills alla dess 171 kapitel är bildlästa, införda rättelser klarar kontrollerna och alla dess sparade läsningsfrågor är avgjorda.** PR #65 behåller sina egna evangeliefrågor och sin befintliga avgränsning. Den nya grenen är tills vidare baserad på evangeliegrenen; när #65 har sammanslagits ska denna PR riktas mot main och vid behov ombaseras.

## Arbetslogg

- **2026-10-26, NT1:** Hela Apg. 1–12 bildjämförda, inklusive alla spalt-/sidfortsättningar. Nitton säkra rättelser R198–R216 bekräftade i originalupplösning och införda. Nio kapitel med 312 poster verifierade; tre med 126 poster väntar på fem sparade frågor V123–V127. Inga verskoordinater eller mappningar ändrade. Tidigare AP-/GT-/evangelieposter behållna. Nästa hela kapitel Apg. 13. Liggare, kandidataudit, kärntest med nitton nya regressioner, sex OSB-metadatatest, 122 resurshashar, fonttäckning och blankstegskontroll godkända. Exakt databasjämförelse bekräftar endast nitton avsedda versändringar, oförändrade övriga texter/koordinater och tidigare liggarposter. R204 återställer uppslagskopplingen utsträcker → utsträcka; övriga lexikondata oförändrade. Alla 119 sparade evangelie-/NT-utsnitt har giltiga hashvärden.
