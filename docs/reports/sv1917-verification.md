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
| Svenska apokryfer | 5 / 12 | 101 / 148 | 2 500 / 4 371 |
| GT utan apokryfer | 1 / 39 | 2 / 929 | 38 / 23 210 |

Bokantalet är inte ett mått på ordmängd: de första fyra apokryfböckerna är korta. Totalt är 2 538 versposter färdiglästa i detta protokoll. Det motsvarar 9,2 % av versposterna i GT och apokryfer tillsammans: 57,2 % av apokryferna och 0,16 % av GT. Det är texttäckning, inte en tidsuppskattning. Inga andra kapitel får räknas som färdiga enbart för att de saknar sökträffar.

| Bok | Färdigläst text | Trycksidor / skannummer | Resultat |
| --- | --- | --- | --- |
| Manasses’ bön | Hela 1:1–15 | 261–262 / [0265](https://runeberg.org/apokryf/0265.html)–[0266](https://runeberg.org/apokryf/0266.html) | Inga nya rättelser |
| Bel och draken | Hela 1:1–42 | 255–257 / [0259](https://runeberg.org/apokryf/0259.html)–[0261](https://runeberg.org/apokryf/0261.html) | A027: lejonssgropen → lejongropen |
| Asarjas bön och De tre männens lovsång | Hela 1:1–65 | 257–260 / [0261](https://runeberg.org/apokryf/0261.html)–[0264](https://runeberg.org/apokryf/0264.html) | A028: slutpunkt i 1:42; tryckets numrering slutar vid 65 |
| Susanna | Hela 1:1–64 | 252–255 / [0256](https://runeberg.org/apokryf/0256.html)–[0259](https://runeberg.org/apokryf/0259.html) | A029–A030: mellanslag i 1:20 och hög röst i 1:24 |
| Haggai | Hela 1:1–14 och 2:1–24 | [1253](https://runeberg.org/bibeln/efs1927/gt/1253.html)–[1255](https://runeberg.org/bibeln/efs1927/gt/1255.html) | R018–R022: till, får och tre interpunktionsfel; behållna kommaskillnader antecknade per kapitel |
| Judit | 1–4 och 6–16; 15 av 16 kapitel, 316 versposter | 1–24 / [0005](https://runeberg.org/apokryf/0005.html)–[0028](https://runeberg.org/apokryf/0028.html) | A031–A036 införda; kapitel 5 bildläst men olöst V003, räknas inte som färdigt |
| Visheten | Hela 1–19, 436 versposter | 24–50 / [0028](https://runeberg.org/apokryf/0028.html)–[0054](https://runeberg.org/apokryf/0054.html) | A037–A060: 24 rättade versposter, inklusive två versgränser |
| Tobit | 1–2 och 4–14; 13 av 14 kapitel, 227 versposter | 50–66 / [0054](https://runeberg.org/apokryf/0054.html)–[0070](https://runeberg.org/apokryf/0070.html) | A061–A080 införda, inklusive återställda 11:19 och 13:18; kapitel 3 väntar på V004 |
| Syrak | 1–37 och 39–51; 50 av 51 kapitel, 1 335 versposter | 67–144 / [0071](https://runeberg.org/apokryf/0071.html)–[0148](https://runeberg.org/apokryf/0148.html) | A081–A145 införda; rättade gränser i 11:30–31, 12:2–4, 13:15–16, 40:15–16 och 47:4–5 samt numrering i 3:26–27. Hela boken bildläst; kapitel 38 väntar på V005 |

## Onumrerad text

**Syraks företal:** hela texten på [tryckta s. 66–67, skannummer 0070–0071](https://runeberg.org/apokryf/0070.html) är ord- och interpunktionsjämförd. De 348 orden i tre stycken är bevarade i [en separat granskad textfil](sv1917-syrak-foretal.txt). Redaktionellt referat och 1:1 ingår inte. JSON-liggarens `unnumbered` anger textfilens hash samt samma datum, granskare och bildbevis som kapitelposterna; kontrollen upptäcker ändrad fil eller saknade bevis. **Filen är rapportunderlag och är inte inläst i appens versdatabas.** Inga kapitel eller versposter tillgodoräknas för den.

## Återstår

**Apokryfer:** Judit 5 och Tobit 3 väntar på bedömning av V003 respektive V004. Syrak 38 väntar på V005. Hela Syrak är bildläst. Baruk, 1 Mackabeerboken, 2 Mackabeerboken och Tillägg till Ester är ännu inte genomlästa kapitelvis i detta protokoll. Enskilda tidigare bildkontroller är dokumenterade i erratatabellen men ger ingen full kapitelstatus.

**GT:** samtliga böcker utom Haggai återstår, även sådana Runeberg historiskt markerade som korrekturlästa. Detta är en konservativ startpunkt: det finns ingen tidigare lokal liggare som bevisar en fullständig genomgång av deras aktuella text.

**Öppna läsningsfrågor:** V001 (Jer. 8:12, någon), V002 (2 Mack. 6:17, avb rrott), V003 (Judit 5:8, förjagade med), V004 (Tobit 3:4, orsdpråk) och V005 (Syrak 38:17, kan) står kvar i erratarapporten. De kapitlen räknas inte som färdiga.

**Omfattning:** svenska 1917/1921-texten, 51 böcker och 27 581 versposter i GT och apokryfer tillsammans. NT och grekiska/engelska utgåvor räknas inte i dessa täckningstal. Ortodoxa extraböcker som saknar svensk text kan inte markeras verifierade genom att kontrollera en annan språkversion.

## Nästa arbetsblock

1. Fortsätt med **Baruk 1** från [skannummer 0149, tryckt s. 145](https://runeberg.org/apokryf/0149.html) och följ hela kapitlet över efterföljande sidor. Syrak är bildläst till bokens slut i 51:30 på skannummer 0148; Baruk är ännu inte tillgodoräknad. Sök även oberoende tryck för de öppna läsningarna V001–V005.
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
- **2026-10-25, block 3:** läste hela Judit, 16 kapitel och 340 versposter på 24 nya faksimilsidor. Sex rättelser A031–A036 införda och återbyggda. Femton kapitel med 316 versposter är färdiga; kapitel 5 med 24 versposter är `needs-review` på grund av tryckets ”förjagade med” (V003). Kontrollerade ovanliga men tryckstödda former och återställda versgränser. Totalt 38 unika bildsidor har lästs i liggarens fulla kapitelgenomgångar; text efter Judits slut räknas inte som läst. Nästa bok är Visheten.
- **2026-10-25, block 4:** läste hela Visheten, 19 kapitel och 436 versposter på 27 faksimilsidor, varav en redan delats med Judit. A037–A060 införda och återbyggda: 24 rättade versposter, inklusive flytt av ord till rätt vers i 14:19–20 och 16:8–9. Alla 19 kapitel färdiga. Totalt 64 unika bildsidor och 976 färdiga versposter. Rättat st järnlågorna avlägsnar falska SALDO-kopplingar till saint och Stockholms-Tidningen; den förväntade mängden lexikonsinnen har uppdaterats från 12 748 till 12 746. Nästa bok är Tobit.
- **2026-10-25, block 5:** läste hela Tobit på 17 faksimilsidor, varav en redan delats med Visheten. A061–A080 införda och återbyggda: 18 ändrade och två återställda versposter, 11:19 och 13:18. Tio föråldrade mappningsundantag borttagna efter kontroll mot samma gränser i grekiska och engelska vittnen. Totalt svensk text innehåller nu 35 517 versposter, varav 4 367 apokryfposter; inga nya ord har lagts till genom versdelningarna. Tretton kapitel med 227 versposter är färdiga; Tobit 3 är bildläst men väntar på V004, tryckets orsdpråk. Totalt 80 unika bildsidor och 1 203 färdiga versposter. Nästa bok är Syrak, inklusive dess ännu oredovisade företal.
- **2026-10-25, block 6:** läste hela Syraks företal och kapitel 1–6, 157 versposter på tryckta s. 66–75. A081–A087 införda och återbyggda: fyra text-/interpunktionsrättelser samt omnumrering av 3:25–26 till tryckets 3:26–27. Sju utgåvemappningar korrigerade. Företalet, 348 ord i tre stycken, bevarat som separat verifierad fil med hash och bildevidens; ingen appintegration eller påhittad versnumrering. Totalt 89 unika bildsidor och 1 360 färdiga versposter i 59 kapitel, plus det separat granskade företalet. Nästa kapitel är Syrak 7.

- **2026-10-25, block 7:** läste hela Syrak 7–12 på tryckta s. 75–84, 152 versposter efter återställning. A088–A099 införda: åtta ord-/interpunktionsrättelser, rättad gräns mellan 11:30–31 och återställda individuella poster för 12:2–4. Tolv mappningsrader korrigerade eller borttagna. Före-/efterjämförelse bekräftar elva ändrade poster och en ny; övriga verser och språkversioner oförändrade. Totalt 98 unika bildsidor och 1 512 färdiga versposter i 65 kapitel, plus företalet. 1 012 kapitel återstår. Nästa kapitel är Syrak 13; V001–V004 kvarstår. Liggar- och kandidattester, core-storage-speech, sex OSB-tester och samtliga 122 resurshashar godkända.

- **2026-10-25, block 8:** läste hela Syrak 13–18 på tryckta s. 84–92, 160 versposter efter återställning. A100–A109 införda: åtta ord-/interpunktionsrättelser samt avskiljning av 13:16 från 13:15 med borttaget främmande kolon. Tolv mappningsrader korrigerade eller borttagna. Före-/efterjämförelse bekräftar nio ändrade poster och en ny; övriga verser och språkversioner oförändrade. Totalt 106 unika bildsidor och 1 672 färdiga versposter i 71 kapitel, plus företalet. 1 006 kapitel återstår. Nästa kapitel är Syrak 19; V001–V004 kvarstår. Liggar- och kandidattester, core-storage-speech, sex OSB-tester och samtliga 122 resurshashar godkända.

- **2026-10-25, block 9:** läste hela Syrak 19–24 på tryckta s. 92–102, 169 versposter. A110–A113 införda: glädes i 19:5, slutkomma i 20:31, borttaget främmande kolon i 23:3 och mätten i 24:19. Kapitel 21–22 utan nya rättelser. Före-/efterjämförelse bekräftar exakt fyra ändrade versposter; alla verskoordinater och övriga texter oförändrade. Två falska SALDO-sinnen för matematik och matmor avlägsnade genom mätten-rättelsen; förväntat antal uppdaterat till 12 744. Totalt 116 unika bildsidor och 1 841 färdiga versposter i 77 kapitel, plus företalet. 1 000 kapitel återstår. Nästa kapitel är Syrak 25; V001–V004 kvarstår. Liggar- och kandidattester, core-storage-speech, sex OSB-tester och samtliga 122 resurshashar godkända.

- **2026-10-25, block 10:** läste hela Syrak 25–30 på tryckta s. 102–110, 154 versposter. A114–A121 införda: fem ord-/orddelningsrättelser och tre interpunktionsrättelser. Kapitel 27 och 30 utan nya rättelser. Före-/efterjämförelse bekräftar exakt åtta ändrade versposter; alla verskoordinater, övriga texter, mappningar och SALDO-sinnen oförändrade. Totalt 124 unika bildsidor och 1 995 färdiga versposter i 83 kapitel, plus företalet. 994 kapitel återstår. Nästa kapitel är Syrak 31; V001–V004 kvarstår. Liggar- och kandidattester, core-storage-speech, sex OSB-tester och samtliga 122 resurshashar godkända.

- **2026-10-25, block 11:** läste hela Syrak 31–36 på tryckta s. 110–118, 173 versposter. A122–A130 införda: tre slutpunkter och sex ord-/accent-/versalkorrigeringar. Kapitel 35 utan nya rättelser. Före-/efterjämförelse bekräftar exakt nio ändrade versposter; alla verskoordinater, övriga texter och mappningar oförändrade. Två falska SALDO-sinnen och en Dalin-post för snäv/snäf avlägsnade genom snava-rättelsen; förväntat antal SALDO-sinnen uppdaterat till 12 742. Totalt 132 unika bildsidor och 2 168 färdiga versposter i 89 kapitel, plus företalet. 988 kapitel återstår. Nästa kapitel är Syrak 37; V001–V004 kvarstår. Liggar- och kandidattester, core-storage-speech, sex OSB-tester och samtliga 122 resurshashar godkända.

- **2026-10-25, block 12:** läste hela Syrak 37–42 på tryckta s. 119–129, 177 versposter efter återställning. A131–A137 införda: fem ord-/interpunktionsrättelser samt avskiljning av 40:16 från 40:15. Tolv mappningsrader korrigerade eller borttagna. Kapitel 38 är helt bildläst men väntar på nya V005, tryckets såsom kan förtjänar i 38:17; övriga fem kapitel ger 143 färdiga versposter. Före-/efterjämförelse bekräftar sex ändrade poster och en ny; övriga texter oförändrade. En falsk SALDO-koppling och en Dalin-post för skiftning borttagna; 12 741 SALDO-sinnen återstår. Totalt 143 unika bildsidor och 2 311 färdiga versposter i 94 kapitel, plus företalet. 983 kapitel återstår. Nästa kapitel är Syrak 43; V001–V005 kvarstår. Liggar- och kandidattester, core-storage-speech, sex OSB-tester och samtliga 122 resurshashar godkända.

- **2026-10-25, block 13:** läste hela Syrak 43–51 på tryckta s. 129–144, 227 versposter efter återställning. A138–A145 införda: sex ord-/versalkorrigeringar samt avskiljning av 47:5 från 47:4 med dens → den i den återställda versen. Tolv mappningsrader korrigerade eller borttagna. Kapitel 44, 48 och 49 utan nya rättelser. Före-/efterjämförelse bekräftar exakt sju ändrade poster och en ny; inga andra texter eller lexikonsinnen ändrade. Hela Syrak och dess företal är nu bildlästa; 50 av 51 kapitel, 1 335 versposter, är verifierade, medan kapitel 38 väntar på V005. Totalt 158 unika bildsidor och 2 538 färdiga versposter i 103 kapitel, plus företalet. 974 kapitel återstår. Nästa bok är Baruk; V001–V005 kvarstår. Liggar- och kandidattester, core-storage-speech, sex OSB-tester och samtliga 122 resurshashar godkända.
