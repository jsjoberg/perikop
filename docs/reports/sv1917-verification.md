# Löpande granskningsliggare för svensk bibeltext

**Uppdaterad 2026-10-25. Aktivt arbete i [utkast-PR #64](https://github.com/jsjoberg/perikop/pull/64). PR:n får inte slås ihop eller stängas förrän hela den svenska utökade kanon och all återstående overifierad GT-text har verifierats.**

Till skillnad från daterade rapporter uppdateras denna liggare under arbetet. Den anger vad som är färdigläst, vad som endast ordkontrollerats och var nästa genomgång börjar. Detaljerna finns i [den maskinläsbara liggaren](sv1917-verification.json).

## Vad färdig betyder

Ett kapitel räknas endast när hela dess bibeltext har jämförts visuellt med angivna trycksidor, inklusive fortsättningar över spalt- och sidgränser, ord som inte flaggats av något sökskript och textens början och slut. Ord, interpunktion och versindelning ingår. Alla funna avvikelser ska vara rättade med bildstöd eller uttryckligen redovisade som behållna skillnader mot bildvittnet. En olöst läsning ger status `needs-review` och räknas inte som färdig.

Radbrytningar, radslutsavstavning, spärrning, kapitälstil, citat-/apostrofglyfer och fotnotsmarkörer är typografisk återgivning. Rubriker, kapitelreferat och noter är inte bibeltext. Den kanoniska e-texten kommer från flera tryckningar: skillnader i kommatering harmoniseras inte automatiskt med EFS 1927, utan antecknas i kapitelposten. Möjliga ordbortfall eller läsningsvarianter behandlas däremot som olösta tills de har bedömts mot lämpliga vittnen.

Varje färdig post anger bok/kapitel, versantal, datum, granskare, anteckningar, SHA-256 för den exakta granskade korpustexten samt URL och SHA-256 för varje TIFF-bild. Kontrollen avvisar ändrad text, dubbla kapitel och saknad bildevidens. Den kan inte själv utföra eller intyga bildläsningen.

Granskare hittills: **Codex, visuell jämförelse**. Din separata kontroll av [erratatabellens](sv1917-errata.md) kryssrutor återstår. Tidigare riktade ordkontroller och Runebergs historiska korrekturstatus räknas inte som hela kapitel i denna liggare. En delad sida ger bara täckning för den bok och det kapitel som faktiskt har lästs.

## Färdigt hittills

| Del | Hela böcker | Hela kapitel | Versposter |
| --- | ---: | ---: | ---: |
| Svenska apokryfer | 4 / 12 | 4 / 148 | 186 / 4 365 |
| GT utan apokryfer | 1 / 39 | 2 / 929 | 38 / 23 210 |

Bokantalet är inte ett mått på ordmängd: de första fyra apokryfböckerna är korta. Totalt är 224 versposter färdiglästa i detta protokoll. Inga andra kapitel får räknas som färdiga enbart för att de saknar sökträffar.

| Bok | Färdigläst text | Trycksidor / skannummer | Resultat |
| --- | --- | --- | --- |
| Manasses’ bön | Hela 1:1–15 | 261–262 / [0265](https://runeberg.org/apokryf/0265.html)–[0266](https://runeberg.org/apokryf/0266.html) | Inga nya rättelser |
| Bel och draken | Hela 1:1–42 | 255–257 / [0259](https://runeberg.org/apokryf/0259.html)–[0261](https://runeberg.org/apokryf/0261.html) | A027: lejonssgropen → lejongropen |
| Asarjas bön och De tre männens lovsång | Hela 1:1–65 | 257–260 / [0261](https://runeberg.org/apokryf/0261.html)–[0264](https://runeberg.org/apokryf/0264.html) | A028: slutpunkt i 1:42; tryckets numrering slutar vid 65 |
| Susanna | Hela 1:1–64 | 252–255 / [0256](https://runeberg.org/apokryf/0256.html)–[0259](https://runeberg.org/apokryf/0259.html) | A029–A030: mellanslag i 1:20 och hög röst i 1:24 |
| Haggai | Hela 1:1–14 och 2:1–24 | [1253](https://runeberg.org/bibeln/efs1927/gt/1253.html)–[1255](https://runeberg.org/bibeln/efs1927/gt/1255.html) | R018–R022: till, får och tre interpunktionsfel; behållna kommaskillnader antecknade per kapitel |

## Återstår

**Apokryfer:** Judit, Visheten, Tobit, Syrak, Baruk, 1 Mackabeerboken, 2 Mackabeerboken och Tillägg till Ester är ännu inte genomlästa kapitelvis i detta protokoll. Enskilda tidigare bildkontroller är dokumenterade i erratatabellen men ger ingen full kapitelstatus.

**GT:** samtliga böcker utom Haggai återstår, även sådana Runeberg historiskt markerade som korrekturlästa. Detta är en konservativ startpunkt: det finns ingen tidigare lokal liggare som bevisar en fullständig genomgång av deras aktuella text.

**Öppna läsningsfrågor:** V001 (Jer. 8:12, någon) och V002 (2 Mack. 6:17, avb rrott) står kvar i erratarapporten. De kapitlen räknas inte som färdiga.

**Omfattning:** svenska 1917/1921-texten, 51 böcker och 27 575 versposter i GT och apokryfer tillsammans. NT och grekiska/engelska utgåvor räknas inte i dessa täckningstal. Ortodoxa extraböcker som saknar svensk text kan inte markeras verifierade genom att kontrollera en annan språkversion.

## Nästa arbetsblock

1. Fortsätt med **Judit 1** från [skannummer 0005, tryckt s. 1](https://runeberg.org/apokryf/0005.html) och följ hela kapitlet över efterföljande sidor. Läs sedan vidare kapitel för kapitel i Judit.
2. Lägg till en kapitelpost först efter full jämförelse och efter att eventuella rättelser har återbyggts i korpusen. `needs-review` används om något fortfarande är olöst.
3. Kör liggarkontrollen och relevanta regressioner. Uppdatera denna sammanställning och PR #64 vid varje färdigt arbetsblock. Lämna ofärdiga kapitel uttryckligen ofärdiga.

```sh
uv run --locked tools/corpus/check_swedish_verification.py
uv run --locked tests/swedish_verification_test.py
```

## Arbetslogg

- **2026-10-25, grundläge:** de tre riktade sökomgångarna gav 43 rättade verser men ingen styrkt full kapitelgenomgång. Dessa fynd finns kvar som riktade kontroller i erratatabellen.
- **2026-10-25, block 1:** läste samtliga verser i Manasses’ bön, Bel och draken, Asarjas bön inklusive lovsången samt Susanna; 11 unika faksimilsidor, fyra hela kapitel och 186 versposter. Fyra nya rättelser A027–A030. Tidigare återställda passager över sidgränser kontrollerades på nytt.
- **2026-10-25, block 2:** läste hela Haggai på tre faksimilsidor; två kapitel och 38 versposter. Fem nya rättelser R018–R022. Kommaskillnader och placeringen av 2:1 före kapitelrubriken dokumenterades. Överlappande Sefanja- och Sakarjatext räknas inte som granskad.
