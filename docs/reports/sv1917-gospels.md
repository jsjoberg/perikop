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
| Matteus | 1–18 | 1–10, 14–17 (14/28) | 444/1 067 |
| Markus | Inga | 0/16 | 0/673 |
| Lukas | Inga | 0/24 | 0/1 149 |
| Johannes | Inga | 0/21 | 0/878 |
| **Evangelierna totalt** | **18/89** | **14/89** | **444/3 767 (11,8 %)** |

Matteus 1–18 omfattar 616 bildlästa versposter och 13 342 ord på 30 unika trycksidor, NT s. 3–32. Av detta är 444 versposter och 9 464 ord i färdiga kapitel. Det motsvarar **11,8 % verifierat av evangeliernas versposter och 11,6 % av deras ordmängd**. Helt bildläst text motsvarar **16,4 % av ordmängden**; evangeliernas totala 81 446 ord är oförändrade i denna omgång. Ord räknas med samma regel som den inledande storleksjämförelsen; tal, rubriker, noter och referenser räknas inte.

**Infört, R024–R028:** for → för i Matt. 1:19 och 2:19; felaktigt stort I → i i 9:17; saknat I återställt i 10:27; avslutande citattecken återställt i 3:15. Förstorad originalbild löste citattecknet utan nytt foto. Fem verser ändrade, alla andra texter och verskoordinater oförändrade; mappningar och lexikonsinnen oförändrade.

**Löst 2026-10-26, V007–V009:** [Användarens tre foton](sv1917-print-witnesses/README.md) bekräftar såsom i Matt. 1:24 (R029) och stannen så hos honom i 10:11 (R030). Foto V008 bekräftar däremot korpusens olika slags lidanden i 4:24, en tryckskillnad mot EFS 1927:s alla; ingen rättelse införs där. Kommat efter han i 1:24 läggs inte till: både fotot och korpusen saknar det. De 92 versposterna i kapitel 1, 4 och 10 kan nu räknas som verifierade eftersom hela kapitlen redan var bildlästa. Ingen fotofråga i första bunten återstår. Första arbetsblocket har därmed gett sju rättelser och en bekräftad behållen ordvariant; det är ett praktiskt prov av arbetsflödet, inte ett slumpat stickprov eller en uppmätt felkvot för hela NT.

**Nytt block, Matteus 11–18:** 301 versposter och 6 574 ord helt bildjämförda på NT s. 17–32. R031–R035 återställer avslutande apostrofer i 13:15 och 13:27, meningspunkten i 14:20, för i 17:27 och litet i i 18:5. Kapitel 14–17 är färdiga: 129 versposter och 2 696 ord. Kapitel 11, 12, 13 och 18 har 172 fullt lästa men ännu inte verifierade versposter; de väntar på V010–V016. Frånvarande löptext vid 17:21 och 18:11 har inte skapats från nothänvisningar.

## Sparat till senare boktillgång

[Fotobunt 2, V010–V016](sv1917-gospel-questions/batch-2.md), bevarar hela korpusverser, läsbara scanutsnitt, sidlänkar, exakta TIFF-/utsnittshashar och vad ett senare foto behöver täcka. Användaren är borta från boken; **ingen åtgärd behövs nu**. Sju frågor kan täckas av fem foton: Matt. 11:10; 11:23; 12:26–27 med versnummer; 13:34 + 13:39; 18:20 + 18:25. Det gäller sex ordforms-/pronomenfrågor och en versgräns, inte en lista med sju fastställda fel. Korpustext och mappningar för dessa frågor behålls. Granskningen fortsätter i andra kapitel medan fotona väntar.

## Återuppta här

Fortsätt med **Matteus 19** från [NT s. 32](https://runeberg.org/bibeln/efs1927/nt/0032.html). Kapitel 19 har inte tillgodoräknats genom den delade sidan. Därefter återstår Matteus 20–28, hela Markus, Lukas och Johannes. **71 evangeliekapitel är ännu olästa**; fyra ytterligare är helt lästa men väntar på den sparade fotobunten. Övriga NT och resterande GT ingår inte i denna etapp.

```sh
uv run --locked tools/corpus/check_swedish_verification.py
uv run --locked tests/swedish_verification_test.py
```

## Arbetslogg

- **2026-10-25, block E1:** Matteus 1–10 helt jämförda mot NT s. 3–17. Sju kapitel färdiga, tre väntande; R024–R028 införda. Fem exakta textändringar, oförändrade verskoordinater, andra texter, språkversioner, mappningar, lexikonsinnen och tidigare liggarposter. Nästa kapitel Matteus 11. Tre neutrala fotofrågor med hela korpusversen och beskurna originalbilder; användaren behöver inte transkribera eller tolka dem. Liggar- och kandidattester, NT-vittnenas regressioner, core-storage-speech, sex OSB-tester, alla 122 resurshashar, korpusens teckentäckning och diffens blankstegskontroll godkända.

- **2026-10-26, block E2:** Codex tolkade tre kompletterande foton. R029 rättar som → såsom i Matt. 1:24; R030 återställer så i 10:11. V008 bekräftar befintligt olika i 4:24. Två verser ändrade, en uttryckligen behållen. Originalfoton sparade byteidentiskt med SHA-256; liggaren stöder även ett behållet läsningsbeslut utan rättelse-ID. De tre redan bildlästa väntande kapitlen färdigmarkerade, 92 versposter; inga nya hela kapitel tillgodoräknade genom fotona. Totalt 10/89 kapitel, 315/3 767 versposter och 6 768/81 446 ord verifierade. Nästa kapitel Matteus 11. PR #64 är sammanslagen; evangeliegrenen ombaserad på main utan textändring från ombaseringen. Liggar- och kandidattester, foto- och behållen-läsningsregressioner, core-storage-speech, sex OSB-tester, alla 122 resurshashar, korpusens teckentäckning och diffens blankstegskontroll godkända. Före-/efterjämförelsen bekräftar exakt två ändrade verser, oförändrade övriga texter, koordinater, mappningar och lexikonsinnen.

- **2026-10-26, block E3:** Hela Matteus 11–18, 301 versposter och 6 574 ord, jämförda mot NT s. 17–32. R031–R035 införda: två avslutande apostrofer, en meningspunkt och två prepositionsrättelser. Originalupplösning använd för samtliga rättade utdrag och för de uppskjutna läsningarna. Kapitel 14–17 färdiga, 129 poster; 11–13 och 18 väntar på sju sparade frågor V010–V016, 172 poster. Frågebunten har åtta scanutsnitt, hela verser och exakta källhashar och kan besvaras med fem foton när användaren åter har boken. Ingen omedelbar insats begärs. Totalt 18/89 kapitel helt bildlästa, 14/89 verifierade; 444/3 767 poster (11,8 %) och 9 464/81 446 ord (11,6 %) verifierade. Nästa kapitel Matteus 19. Liggar- och kandidattester, core-storage-speech med de fem nya regressionerna, sex OSB-tester, alla 122 resurshashar, korpusens teckentäckning och diffens blankstegskontroll godkända. Före-/efterjämförelsen bekräftar exakt fem ändrade versposter; alla andra texter, koordinater, mappningar, lexikonsinnen och tidigare liggarposter är oförändrade. Även alla uppskjutna frågors läsningar är oförändrade. Samtliga åtta sparade utsnitt stämmer med angivna SHA-256.
