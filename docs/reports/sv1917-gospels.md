# Evangelierna: löpande bildgranskning

**Uppdaterad 2026-10-26. Aktiv gren: `codex/sv1917-gospels-verification`. Aktiv PR #65 med main som bas, efter den sammanslagna PR #64 för utökade kanon; ingen utvidgning av dess 0.2.1-villkor.**

Målet är full jämförelse av den svenska 1917-texten i Matteus, Markus, Lukas och Johannes mot EFS 1927:s tryckbilder: 89 kapitel och 3 767 versposter. Samma krav på full kapiteltext, spalt-/sidfortsättningar, ord, skiljetecken och versgränser gäller som i [huvudliggaren](sv1917-verification.md). Exakt kapiteltext, TIFF-adresser och SHA-256 sparas i den gemensamma [JSON-liggaren](sv1917-verification.json); tidigare verifierade AP-/GT-poster ändras inte.

## Litet arbete för användaren

1. Codex läser hela kapitlet och försöker lösa misstänkta läsningar genom originalupplösning, omgivande text och tillgängliga tryckbilder.
2. Säkra transkriptionsfel rättas med versbundna ersättningar och regressioner. Möjliga ordvarianter eller tryckfel får ett stabilt V-ID och en liten fotobunt med hela versen, full sidlänk och ett läsbart scanutsnitt.
3. Användaren kan svara med foton av de angivna verserna från sitt exemplar. Ingen transkription, egen feldiagnos eller markering behövs. Bibliografiska uppgifter är välkomna men inget villkor för att läsa bilden.
4. Codex tolkar fotona och dokumenterar slutsatser och bevis. Bara kvarstående oklarhet tas tillbaka till användaren. Andra kapitel läses vidare medan en fråga väntar.
5. Ett helt läst kapitel med olöst fråga står som `needs-review` och räknas inte som verifierat. En kompletterande versbild gör inte omgivande oläst text verifierad. Ursprungliga foton och deras hash bevaras när de kommer.

Vanliga kommaskillnader harmoniseras inte automatiskt mellan tryckningar och läggs inte var för sig i fotokön. De antecknas per kapitel; ordskillnader som kan ändra läsningen hålls däremot öppna. Detta undviker att förväxla tryckvariation med ett säkert transkriptionsfel.

## Aktuell status

| Bok | Helt bildlästa kapitel | Verifierade kapitel | Verifierade versposter |
| --- | ---: | ---: | ---: |
| Matteus | 1–28 | 1–10, 14–17, 19, 21–23, 26–28 (21/28) | 764/1 067 |
| Markus | Inga | 0/16 | 0/673 |
| Lukas | Inga | 0/24 | 0/1 149 |
| Johannes | Inga | 0/21 | 0/878 |
| **Evangelierna totalt** | **28/89** | **21/89** | **764/3 767 (20,3 %)** |

Hela Matteus är nu bildläst: 28 kapitel, 1 067 versposter och 22 646 ord på 52 unika trycksidor, NT s. 3–54. Av detta är 764 versposter och 16 063 ord i färdiga kapitel. Det motsvarar **20,3 % verifierat av evangeliernas versposter och 19,7 % av ordmängden**. Helt bildläst text motsvarar **27,8 % av ordmängden**; evangeliernas totala 81 446 ord är oförändrade. Matteus är ännu inte helt verifierat: sju kapitel med 303 versposter väntar på sparade frågor. Ord räknas med samma regel som den inledande storleksjämförelsen; tal, rubriker, noter och referenser räknas inte.

**Infört, R024–R028:** for → för i Matt. 1:19 och 2:19; felaktigt stort I → i i 9:17; saknat I återställt i 10:27; avslutande citattecken återställt i 3:15. Förstorad originalbild löste citattecknet utan nytt foto. Fem verser ändrade, alla andra texter och verskoordinater oförändrade; mappningar och lexikonsinnen oförändrade.

**Löst 2026-10-26, V007–V009:** [Användarens tre foton](sv1917-print-witnesses/README.md) bekräftar såsom i Matt. 1:24 (R029) och stannen så hos honom i 10:11 (R030). Foto V008 bekräftar däremot korpusens olika slags lidanden i 4:24, en tryckskillnad mot EFS 1927:s alla; ingen rättelse införs där. Kommat efter han i 1:24 läggs inte till: både fotot och korpusen saknar det. De 92 versposterna i kapitel 1, 4 och 10 kan nu räknas som verifierade eftersom hela kapitlen redan var bildlästa. Ingen fotofråga i första bunten återstår. Första arbetsblocket har därmed gett sju rättelser och en bekräftad behållen ordvariant; det är ett praktiskt prov av arbetsflödet, inte ett slumpat stickprov eller en uppmätt felkvot för hela NT.

**Tidigare block, Matteus 11–18:** 301 versposter och 6 574 ord helt bildjämförda på NT s. 17–32. R031–R035 återställer avslutande apostrofer i 13:15 och 13:27, meningspunkten i 14:20, för i 17:27 och litet i i 18:5. Kapitel 14–17 är färdiga: 129 versposter och 2 696 ord. Kapitel 11, 12, 13 och 18 har 172 fullt lästa men ännu inte verifierade versposter; de väntar på V010–V016. Frånvarande löptext vid 17:21 och 18:11 har inte skapats från nothänvisningar.

**Nytt block, Matteus 19–28:** 451 versposter och 9 304 ord helt bildjämförda på NT s. 32–54. R036–R041 rättar främmande punkt i 20:11, främmande citatavslut i 21:24, punkt till komma i 21:42 och 23:23, då då → då gå i 25:46 och borde → hörde i 27:47. Sju kapitel tillför 320 verifierade versposter. Originalupplösningen bekräftar hela namnet Barabbas i 27:21; inget foto behövs där. Kapitel 20, 24 och 25 väntar på fyra nya frågor V017–V020. Frånvarande löptext vid 21:44 och 23:14 skapas inte från nothänvisningar.

## Sparat till senare boktillgång

[Fotobunt 2, V010–V016](sv1917-gospel-questions/batch-2.md), och [fotobunt 3, V017–V020](sv1917-gospel-questions/batch-3.md), bevarar hela korpusverser, läsbara scanutsnitt, sidlänkar, exakta TIFF-/utsnittshashar och vad senare foton behöver täcka. Användaren är borta från boken; **ingen åtgärd behövs nu**. Totalt elva öppna frågor i sju Matteuskapitel: 11–13, 18, 20, 24–25. Detta är inte elva fastställda fel. Korpustext och mappningar för frågorna behålls. Varje bunt anger vilka verser som kan samlas i samma foto.

## Återuppta här

Fortsätt med **Markus 1** från [NT s. 55](https://runeberg.org/bibeln/efs1927/nt/0055.html). Hela Matteus är bildläst; sparade frågor avgörs när kompletterande foton finns. **61 evangeliekapitel saknar ännu färdig kapitelgenomgång i liggaren.** Användaren har uttryckligen godkänt att nästa avsnitt/bok påbörjas utan ny uppmaning. Fortsätt därför Markus, Lukas och Johannes medan frågorna väntar. Övriga NT och resterande GT ingår inte i denna PR.

```sh
uv run --locked tools/corpus/check_swedish_verification.py
uv run --locked tests/swedish_verification_test.py
```

## Arbetslogg

- **2026-10-25, block E1:** Matteus 1–10 helt jämförda mot NT s. 3–17. Sju kapitel färdiga, tre väntande; R024–R028 införda. Fem exakta textändringar, oförändrade verskoordinater, andra texter, språkversioner, mappningar, lexikonsinnen och tidigare liggarposter. Nästa kapitel Matteus 11. Tre neutrala fotofrågor med hela korpusversen och beskurna originalbilder; användaren behöver inte transkribera eller tolka dem. Liggar- och kandidattester, NT-vittnenas regressioner, core-storage-speech, sex OSB-tester, alla 122 resurshashar, korpusens teckentäckning och diffens blankstegskontroll godkända.

- **2026-10-26, block E2:** Codex tolkade tre kompletterande foton. R029 rättar som → såsom i Matt. 1:24; R030 återställer så i 10:11. V008 bekräftar befintligt olika i 4:24. Två verser ändrade, en uttryckligen behållen. Originalfoton sparade byteidentiskt med SHA-256; liggaren stöder även ett behållet läsningsbeslut utan rättelse-ID. De tre redan bildlästa väntande kapitlen färdigmarkerade, 92 versposter; inga nya hela kapitel tillgodoräknade genom fotona. Totalt 10/89 kapitel, 315/3 767 versposter och 6 768/81 446 ord verifierade. Nästa kapitel Matteus 11. PR #64 är sammanslagen; evangeliegrenen ombaserad på main utan textändring från ombaseringen. Liggar- och kandidattester, foto- och behållen-läsningsregressioner, core-storage-speech, sex OSB-tester, alla 122 resurshashar, korpusens teckentäckning och diffens blankstegskontroll godkända. Före-/efterjämförelsen bekräftar exakt två ändrade verser, oförändrade övriga texter, koordinater, mappningar och lexikonsinnen.

- **2026-10-26, block E3:** Hela Matteus 11–18, 301 versposter och 6 574 ord, jämförda mot NT s. 17–32. R031–R035 införda: två avslutande apostrofer, en meningspunkt och två prepositionsrättelser. Originalupplösning använd för samtliga rättade utdrag och för de uppskjutna läsningarna. Kapitel 14–17 färdiga, 129 poster; 11–13 och 18 väntar på sju sparade frågor V010–V016, 172 poster. Frågebunten har åtta scanutsnitt, hela verser och exakta källhashar och kan besvaras med fem foton när användaren åter har boken. Ingen omedelbar insats begärs. Totalt 18/89 kapitel helt bildlästa, 14/89 verifierade; 444/3 767 poster (11,8 %) och 9 464/81 446 ord (11,6 %) verifierade. Nästa kapitel Matteus 19. Liggar- och kandidattester, core-storage-speech med de fem nya regressionerna, sex OSB-tester, alla 122 resurshashar, korpusens teckentäckning och diffens blankstegskontroll godkända. Före-/efterjämförelsen bekräftar exakt fem ändrade versposter; alla andra texter, koordinater, mappningar, lexikonsinnen och tidigare liggarposter är oförändrade. Även alla uppskjutna frågors läsningar är oförändrade. Samtliga åtta sparade utsnitt stämmer med angivna SHA-256.

- **2026-10-26, block E4:** Hela Matteus 19–28, 451 versposter och 9 304 ord, jämförda på NT s. 32–54. R036–R041 införda. Sju kapitel tillför 320 verifierade versposter; 20, 24 och 25 väntar på fyra nya frågor V017–V020, sparade med fyra scanutsnitt och fulla verser. Barabbas i 27:21 löst genom originalupplösning utan foto. Hela Matteus bildläst; 21/28 kapitel och 764/1 067 poster verifierade, sju kapitel väntande. Evangeliernas verifierade täckning 764/3 767 (20,3 %) och 16 063/81 446 ord (19,7 %). Nästa bok Markus; fortsättning är godkänd utan ny användaruppmaning. Liggar- och kandidattester, core-storage-speech med de sex nya regressionerna, sex OSB-tester, alla 122 resurshashar, korpusens teckentäckning och diffens blankstegskontroll godkända. Före-/efterjämförelsen bekräftar exakt sex ändrade versposter; övriga texter, koordinater, mappningar, metadata, lexikonsinnen och tidigare liggarposter oförändrade. Samtliga tolv sparade frågeutsnitt matchar angivna SHA-256.
