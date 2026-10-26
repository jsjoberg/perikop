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

## Första blockets resultat

| Bok | Helt bildlästa kapitel | Verifierade kapitel | Verifierade versposter |
| --- | ---: | ---: | ---: |
| Matteus | 1–10 | 1–10 (10/28) | 315/1 067 |
| Markus | Inga | 0/16 | 0/673 |
| Lukas | Inga | 0/24 | 0/1 149 |
| Johannes | Inga | 0/21 | 0/878 |
| **Evangelierna totalt** | **10/89** | **10/89** | **315/3 767 (8,4 %)** |

Matteus 1–10 har 315 versposter och 6 768 ord på 15 unika trycksidor, NT s. 3–17. Alla dessa kapitel är verifierade efter den kompletterande fotobunten. Det motsvarar **8,4 % av evangeliernas versposter och 8,3 % av deras ordmängd**, som nu är 81 446 ord efter återställningarna av I i Matt. 10:27 och så i 10:11. Ord räknas med samma regel som den inledande storleksjämförelsen; tal, rubriker, noter och referenser räknas inte.

**Infört, R024–R028:** for → för i Matt. 1:19 och 2:19; felaktigt stort I → i i 9:17; saknat I återställt i 10:27; avslutande citattecken återställt i 3:15. Förstorad originalbild löste citattecknet utan nytt foto. Fem verser ändrade, alla andra texter och verskoordinater oförändrade; mappningar och lexikonsinnen oförändrade.

**Löst 2026-10-26, V007–V009:** [Användarens tre foton](sv1917-print-witnesses/README.md) bekräftar såsom i Matt. 1:24 (R029) och stannen så hos honom i 10:11 (R030). Foto V008 bekräftar däremot korpusens olika slags lidanden i 4:24, en tryckskillnad mot EFS 1927:s alla; ingen rättelse införs där. Kommat efter han i 1:24 läggs inte till: både fotot och korpusen saknar det. De 92 versposterna i kapitel 1, 4 och 10 kan nu räknas som verifierade eftersom hela kapitlen redan var bildlästa. Ingen fotofråga väntar. Första arbetsblocket har därmed gett sju rättelser och en bekräftad behållen ordvariant; det är ett praktiskt prov av arbetsflödet, inte ett slumpat stickprov eller en uppmätt felkvot för hela NT.

## Återuppta här

Fortsätt med **Matteus 11** från [NT s. 17](https://runeberg.org/bibeln/efs1927/nt/0017.html). På den delade sidan är endast texten till och med 10:42 tillgodoräknad; 11:1 är ännu inte fullständigt granskat. Därefter återstår Matteus 12–28, hela Markus, Lukas och Johannes. 79 evangeliekapitel är ännu olästa; inga lästa kapitel väntar på kompletterande foto. Övriga NT och resterande GT ingår inte i denna etapp.

```sh
uv run --locked tools/corpus/check_swedish_verification.py
uv run --locked tests/swedish_verification_test.py
```

## Arbetslogg

- **2026-10-25, block E1:** Matteus 1–10 helt jämförda mot NT s. 3–17. Sju kapitel färdiga, tre väntande; R024–R028 införda. Fem exakta textändringar, oförändrade verskoordinater, andra texter, språkversioner, mappningar, lexikonsinnen och tidigare liggarposter. Nästa kapitel Matteus 11. Tre neutrala fotofrågor med hela korpusversen och beskurna originalbilder; användaren behöver inte transkribera eller tolka dem. Liggar- och kandidattester, NT-vittnenas regressioner, core-storage-speech, sex OSB-tester, alla 122 resurshashar, korpusens teckentäckning och diffens blankstegskontroll godkända.

- **2026-10-26, block E2:** Codex tolkade tre kompletterande foton. R029 rättar som → såsom i Matt. 1:24; R030 återställer så i 10:11. V008 bekräftar befintligt olika i 4:24. Två verser ändrade, en uttryckligen behållen. Originalfoton sparade byteidentiskt med SHA-256; liggaren stöder även ett behållet läsningsbeslut utan rättelse-ID. De tre redan bildlästa väntande kapitlen färdigmarkerade, 92 versposter; inga nya hela kapitel tillgodoräknade genom fotona. Totalt 10/89 kapitel, 315/3 767 versposter och 6 768/81 446 ord verifierade. Nästa kapitel Matteus 11. PR #64 är sammanslagen; evangeliegrenen ombaserad på main utan textändring från ombaseringen. Liggar- och kandidattester, foto- och behållen-läsningsregressioner, core-storage-speech, sex OSB-tester, alla 122 resurshashar, korpusens teckentäckning och diffens blankstegskontroll godkända. Före-/efterjämförelsen bekräftar exakt två ändrade verser, oförändrade övriga texter, koordinater, mappningar och lexikonsinnen.
