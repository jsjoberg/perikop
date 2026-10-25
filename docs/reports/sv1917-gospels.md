# Evangelierna: löpande bildgranskning

**Uppdaterad 2026-10-25. Aktiv gren: `codex/sv1917-gospels-verification`. Separat fortsättning efter den färdiga utökade kanon i PR #64; ingen utvidgning av dess 0.2.1-villkor.**

Målet är full jämförelse av den svenska 1917-texten i Matteus, Markus, Lukas och Johannes mot EFS 1927:s tryckbilder: 89 kapitel och 3 767 versposter. Samma krav på full kapiteltext, spalt-/sidfortsättningar, ord, skiljetecken och versgränser gäller som i [huvudliggaren](sv1917-verification.md). Exakt kapiteltext, TIFF-adresser och SHA-256 sparas i den gemensamma [JSON-liggaren](sv1917-verification.json); tidigare verifierade AP-/GT-poster ändras inte.

## Litet arbete för användaren

1. Codex läser hela kapitlet och försöker lösa misstänkta läsningar genom originalupplösning, omgivande text och tillgängliga tryckbilder.
2. Säkra transkriptionsfel rättas med versbundna ersättningar och regressioner. Möjliga ordvarianter eller tryckfel får ett stabilt V-ID och en liten fotobunt med hela versen, full sidlänk och ett läsbart scanutsnitt.
3. Användaren kan svara med foton av de angivna verserna från sitt exemplar. Ingen transkription, egen feldiagnos eller markering behövs. Bibliografiska uppgifter är välkomna men inget villkor för att läsa bilden.
4. Codex tolkar fotona och dokumenterar slutsatser och bevis. Bara kvarstående oklarhet tas tillbaka till användaren. Andra kapitel läses vidare medan en fråga väntar.
5. Ett helt läst kapitel med olöst fråga står som `needs-review` och räknas inte som verifierat. En kompletterande versbild gör inte omgivande oläst text verifierad. Ursprungliga foton och deras hash bevaras när de kommer.

Vanliga kommaskillnader harmoniseras inte automatiskt mellan tryckningar och läggs inte var för sig i fotokön. De antecknas per kapitel; ordskillnader som kan ändra läsningen hålls däremot öppna. Detta undviker att förväxla tryckvariation med ett säkert transkriptionsfel.

## Första blockets resultat

| Bok | Helt bildlästa kapitel | Verifierade kapitel | Verifierade versposter |
| --- | ---: | ---: | ---: |
| Matteus | 1–10 | 2, 3, 5–9 (7/28) | 223/1 067 |
| Markus | Inga | 0/16 | 0/673 |
| Lukas | Inga | 0/24 | 0/1 149 |
| Johannes | Inga | 0/21 | 0/878 |
| **Evangelierna totalt** | **10/89** | **7/89** | **223/3 767 (5,9 %)** |

Matteus 1–10 har 315 versposter och 6 767 ord på 15 unika trycksidor, NT s. 3–17. Av detta är 223 versposter och 4 896 ord i färdiga kapitel. Det motsvarar **8,3 % helt bildläst respektive 6,0 % verifierat av evangeliernas ordmängd**, som nu är 81 445 ord efter återställningen av I i Matt. 10:27. Ord räknas med samma regel som den inledande storleksjämförelsen; tal, rubriker, noter och referenser räknas inte.

**Infört, R024–R028:** for → för i Matt. 1:19 och 2:19; felaktigt stort I → i i 9:17; saknat I återställt i 10:27; avslutande citattecken återställt i 3:15. Förstorad originalbild löste citattecknet utan nytt foto. Fem verser ändrade, alla andra texter och verskoordinater oförändrade; mappningar och lexikonsinnen oförändrade.

**Väntar, 92 versposter:** Matteus 1, 4 och 10 är fullt bildlästa men inte färdigmarkerade. [Fotobunt 1: V007–V009](sv1917-gospel-questions/README.md) gäller Matt. **1:24**, **4:24** och **10:11**. Vi behåller korpustexten tills ytterligare tryck har bedömts. Detta första arbetsblock gav fem säkra rättelser och tre fotofrågor; det är ett praktiskt prov av arbetsflödet, inte ett slumpat stickprov eller en uppmätt felkvot för hela NT.

## Återuppta här

Fortsätt med **Matteus 11** från [NT s. 17](https://runeberg.org/bibeln/efs1927/nt/0017.html). På den delade sidan är endast texten till och med 10:42 tillgodoräknad; 11:1 är ännu inte fullständigt granskat. Därefter återstår Matteus 12–28, hela Markus, Lukas och Johannes, samt de tre väntande frågorna ovan. 79 evangeliekapitel är ännu olästa; tre ytterligare är lästa men väntande. Övriga NT och resterande GT ingår inte i denna etapp.

```sh
uv run --locked tools/corpus/check_swedish_verification.py
uv run --locked tests/swedish_verification_test.py
```

## Arbetslogg

- **2026-10-25, block E1:** Matteus 1–10 helt jämförda mot NT s. 3–17. Sju kapitel färdiga, tre väntande; R024–R028 införda. Fem exakta textändringar, oförändrade verskoordinater, andra texter, språkversioner, mappningar, lexikonsinnen och tidigare liggarposter. Nästa kapitel Matteus 11. Tre neutrala fotofrågor med hela korpusversen och beskurna originalbilder; användaren behöver inte transkribera eller tolka dem. Liggar- och kandidattester, NT-vittnenas regressioner, core-storage-speech, sex OSB-tester, alla 122 resurshashar, korpusens teckentäckning och diffens blankstegskontroll godkända.
