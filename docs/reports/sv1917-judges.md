# SV1917: Domarboken, full bildgranskning

**2026-10-31 — Codex.** Alla 21 kapitel och 618 versposter jämförda visuellt mot EFS 1927, tryckta s. 333–369. Orden, interpunktionen, versnumren och fortsättningarna över spalt- och sidgränser ingår, även text utan OCR-flaggor. EFS Stockholm 1926, s. 305–337, användes som ett andra fysiskt tryckvittne: faktiska bilder öppnades för rättelser och variantbeslut. Inga olösta läsningsfrågor återstår för Domarboken.

JDG001–JDG168 rättar exakt **168 versposter**; ingen vers läggs till, tas bort eller får annan koordinat. [Erratatabellen](sv1917-errata-judges.md) visar exakta före/efter-utdrag och 168 **omarkerade** rutor för separat mänsklig kontroll. [JSON-liggaren](sv1917-verification.json) binder varje kapitel till den granskade korpustextens SHA-256 och bildbevis. [Bildmanifestet](sv1917-judges-witnesses/manifest.json) binder alla 70 oförändrade bildfiler till URL och SHA-256.

## Metod och begränsning

Den fulla visuella genomgången gjordes mot 1927-bilderna. 1926-bilderna användes för att avgöra varje föreslagen rättelse och varje relevant tryckskillnad; deras trånga marginaler ger ingen rätt att hitta på bortskuren text. OCR användes som kompletterande kandidatkontroll efter bildpasset, aldrig som belägg för kapitelstatus. Felaktiga OCR-förslag, till exempel punkt efter Kaleb i 1:15 och dubbelt citattecken i 11:19, infördes inte.

Båda tryckvittnena stöder varje införd ändring. Skillnader där nuvarande läsning stöds av ett vittne behålls; ingen kommatering harmoniseras till bara 1927 eller bara 1926. Radslutsavstavning, kapitäl-Herren, citatglyfers form, spärrning och fotnotsmarkörer behandlas som typografi. Rubriker, referat, korshänvisningar och noter är inte verslöptext. Ingen text i angränsande Josua eller Rut tillgodoräknas.

Detta är en separat gren från `origin/main` vid `a714eb6`. Mergad täckning före denna gren: apokryfernas 148 kapitel/4 372 versposter och Haggais 2 kapitel/38 versposter. Genesis #72, Exodus #73, Leviticus #75, Numbers #76, Deuteronomy #77 och NT #65/#68/#71 är separata öppna PR:er och räknas inte som mergade här; de ändras inte av denna gren. Efter Domarboken visar denna grens liggare GT 23/929 kapitel och 656/23 210 versposter; totalt GT+apokryfer 171 kapitel och 5 028 versposter.

## Kapitel och bevarade tryckskillnader

| Kapitel | Versposter | EFS 1927 | EFS 1926 | Rättade versposter | Status |
| --- | ---: | --- | --- | ---: | --- |
| 1 | 36 | [333–335](https://runeberg.org/bibeln/efs1927/gt/0333.html) | [305–307](https://archive.org/details/bibelnellerdenhe0000unse/page/n316/mode/1up) | 5 | verified |
| 2 | 23 | [335–336](https://runeberg.org/bibeln/efs1927/gt/0335.html) | [307–308](https://archive.org/details/bibelnellerdenhe0000unse/page/n318/mode/1up) | 7 | verified |
| 3 | 31 | [336–338](https://runeberg.org/bibeln/efs1927/gt/0336.html) | [308–309](https://archive.org/details/bibelnellerdenhe0000unse/page/n319/mode/1up) | 10 | verified |
| 4 | 24 | [338–339](https://runeberg.org/bibeln/efs1927/gt/0338.html) | [309–310](https://archive.org/details/bibelnellerdenhe0000unse/page/n320/mode/1up) | 6 | verified |
| 5 | 31 | [339–341](https://runeberg.org/bibeln/efs1927/gt/0339.html) | [311–312](https://archive.org/details/bibelnellerdenhe0000unse/page/n322/mode/1up) | 6 | verified |
| 6 | 40 | [341–343](https://runeberg.org/bibeln/efs1927/gt/0341.html) | [312–315](https://archive.org/details/bibelnellerdenhe0000unse/page/n323/mode/1up) | 15 | verified |
| 7 | 25 | [343–345](https://runeberg.org/bibeln/efs1927/gt/0343.html) | [315–316](https://archive.org/details/bibelnellerdenhe0000unse/page/n326/mode/1up) | 6 | verified |
| 8 | 35 | [345–347](https://runeberg.org/bibeln/efs1927/gt/0345.html) | [316–318](https://archive.org/details/bibelnellerdenhe0000unse/page/n327/mode/1up) | 8 | verified |
| 9 | 57 | [347–350](https://runeberg.org/bibeln/efs1927/gt/0347.html) | [318–321](https://archive.org/details/bibelnellerdenhe0000unse/page/n329/mode/1up) | 16 | verified |
| 10 | 18 | [350–351](https://runeberg.org/bibeln/efs1927/gt/0350.html) | [321–321](https://archive.org/details/bibelnellerdenhe0000unse/page/n332/mode/1up) | 3 | verified |
| 11 | 40 | [351–353](https://runeberg.org/bibeln/efs1927/gt/0351.html) | [321–323](https://archive.org/details/bibelnellerdenhe0000unse/page/n332/mode/1up) | 9 | verified |
| 12 | 15 | [353–354](https://runeberg.org/bibeln/efs1927/gt/0353.html) | [323–324](https://archive.org/details/bibelnellerdenhe0000unse/page/n334/mode/1up) | 2 | verified |
| 13 | 25 | [354–355](https://runeberg.org/bibeln/efs1927/gt/0354.html) | [324–325](https://archive.org/details/bibelnellerdenhe0000unse/page/n335/mode/1up) | 14 | verified |
| 14 | 20 | [355–357](https://runeberg.org/bibeln/efs1927/gt/0355.html) | [325–327](https://archive.org/details/bibelnellerdenhe0000unse/page/n336/mode/1up) | 3 | verified |
| 15 | 20 | [357–358](https://runeberg.org/bibeln/efs1927/gt/0357.html) | [327–328](https://archive.org/details/bibelnellerdenhe0000unse/page/n338/mode/1up) | 6 | verified |
| 16 | 31 | [358–360](https://runeberg.org/bibeln/efs1927/gt/0358.html) | [328–330](https://archive.org/details/bibelnellerdenhe0000unse/page/n339/mode/1up) | 11 | verified |
| 17 | 13 | [360–361](https://runeberg.org/bibeln/efs1927/gt/0360.html) | [330–330](https://archive.org/details/bibelnellerdenhe0000unse/page/n341/mode/1up) | 6 | verified |
| 18 | 31 | [361–363](https://runeberg.org/bibeln/efs1927/gt/0361.html) | [330–332](https://archive.org/details/bibelnellerdenhe0000unse/page/n341/mode/1up) | 9 | verified |
| 19 | 30 | [363–364](https://runeberg.org/bibeln/efs1927/gt/0363.html) | [332–334](https://archive.org/details/bibelnellerdenhe0000unse/page/n343/mode/1up) | 7 | verified |
| 20 | 48 | [365–367](https://runeberg.org/bibeln/efs1927/gt/0365.html) | [334–336](https://archive.org/details/bibelnellerdenhe0000unse/page/n345/mode/1up) | 14 | verified |
| 21 | 25 | [367–369](https://runeberg.org/bibeln/efs1927/gt/0367.html) | [336–337](https://archive.org/details/bibelnellerdenhe0000unse/page/n347/mode/1up) | 5 | verified |

Följande beslut redovisar kvarvarande skillnader mot vittnena. En bevarad skillnad är inte en olöst läsning; stödet anges uttryckligen. Endast de ändrade utdragen i erratatabellen är rättelser.

- **1:** 1:3 tåga och 1:14 inledande citattecken stöds av 1926 (1927: taga respektive saknat citattecken). Komman i 1:9,10,12,19 behålls enligt 1926; 1927 har ytterligare komman.
- **2:** 2:2 deras och utropstecknet efter gjort behålls med 1926-stöd; 1927 har dess och inget utropstecken. Kommaskillnader i 2:1,10,17,21 behålls enligt 1926.
- **3:** 3:1 har slutpunkt i båda trycken; tankstrecket ligger vid 3:2 i 1927 men vid slutet av 3:1 i 1926. E-textens versgräns behålls. 3:19 har färre komman enligt 1926; kommat efter moabiterna i 3:29 stöds av 1926. 3:20:s komma efter sommarsalen stöds av 1927.
- **4:** Komman i 4:7 (efter skaror), 4:9 och 4:13 harmoniseras inte mellan trycken. 4:21 utan och före gick stöds av 1926; 1927 har och gick. 4:22:s vers börjar efter 4:21 på samma spalt.
- **5:** 5:14 Ned ifrån stöds av 1926; 1927 har Ned från. Komma efter Isaskar och semikolon efter Barak i 5:15 behålls enligt 1926 (1927 har inget komma respektive kolon). Kommat före som i 5:24 finns bara i 1927 och införs inte.
- **6:** 6:13 har avslutande inre citattecken efter Egypten i 1926; det behålls trots bortfall i 1927. 6:15 Ack Herre utan extra komma, 6:18 härifrån, förrän, 6:30 dö; ty, 6:31 alla som och 6:34 Andes stöds av 1926. 1927 har bland annat andes. Komman i 6:17 och 6:26 harmoniseras inte.
- **7:** 7:13 så liggande, 7:14 israeliten och 7:19 Så kommo stöds av 1926; 1927 har liggande, Israeliten och Då kommo. Komman i 7:1,4,6,7,19,25 behålls när något tryck stöder dem; endast kommat efter Seebspressen i 7:25 tas bort eftersom båda saknar det.
- **8:** Kommaskillnader i 8:4,5,8,26,35 behålls med tryckstöd. 8:5:s komma efter mig följer 1926, där 1927 har semikolon. 8:15:s saden behålls (OCR-förslaget sade ger ingen ändring). 8:25 fortsätter över sidan till 347.
- **9:** 9:16:s tankstreck vid versslutet stöds av 1926; 1927 placerar det vid 9:17:s början. 9:24:s komma efter skedde och 9:35:s semikolon efter stadsporten stöds av 1926. Kommaskillnader i 9:1,25,28,44,48,49 behålls. Ordformer och bildstött interpunktion i samtliga 57 verser jämförda.
- **10:** 10:4:s semikolon efter åsnor och 10:15:s nu stöds av 1926; 1927 har komma respektive saknar nu. Komman före som i 10:8 och 10:14 följer 1926 utan extra komma.
- **11:** 11:12:s avslutande citattecken stöds av 1926 men saknas i 1927. 11:18–19:s versgräns följer 1927: ty Arnon är Moabs gräns hör till 18; 1926 trycker 19 före dessa ord. 11:19:s inre enkla citattecken stöds av 1926; 1927 öppnar med dubbelt tecken. 11:26 Aror behålls; 11:33 har Aroer. 11:31:s vadhelst stöds av 1927 (1926: vad helst). Kommaskillnader i 11:2,28,39 behålls.
- **12:** 12:4 ifrån stöds av 1926 (1927: från). Citatglyfer i 12:6 återges enligt e-texten; texten schibbolet/sibbolet och versnumren kontrollerade. 12:12:s komma efter Ajalon stöds av 1927 och behålls.
- **13:** 13:7:s komma efter nasir, 13:8:s Ack Herre utan extra komma och uteblivet komma efter gossen, 13:11:s semikolon/komma samt 13:20:s upp, i lågan behålls med stöd av minst ett tryck. 13:12 rättas till Huru enligt båda. Extra komman i 13:14 införs inte.
- **14:** 14:6:s komma efter honom och 14:17:s på gåtan stöds av 1926; 1927 saknar dem. Kommat efter HERREN i 14:4 stöds av 1927. Övriga kommaskillnader i 14:19 behålls utan harmonisering.
- **15:** 15:1:s semikolon efter in stöds av 1927 (1926: komma). Kommaskillnader i 15:6,12,15 behålls enligt 1926 utan extra komman. 15:15 har tusen man i båda trycken, inte män.
- **16:** 16:7,11 såsom, 16:10,13 huru, 16:12:s !»; och, 16:15:s nu och 16:29:s mittelpelare stöds av 1926; 1927 har som, hur, ny mening med Och, saknar nu respektive mittpelare. 16:23:s gud stöds av 1926 (1927: Gud). Kommaskillnader i 16:3,4,20,26,30 behålls med tryckstöd.
- **17:** 17:1:s komma efter man bekräftat i båda bilderna. 17:6:s frånvaro av komma efter gjorde stöds av 1927. 17:9:s komma efter se bekräftat på bilden även om 1926-OCR läste semikolon. 17:10–11:s gräns och sidfortsättning kontrollerade.
- **18:** 18:14 hade och 18:22 de män stöds av 1926 men saknas i 1927. 18:19:s avslutande citattecken stöds av 1926. 18:27 utan komma efter tagit stöds av 1927. Kommat efter Dan i 18:29 stöds av 1927. Övriga kommaskillnader i 18:10,22 behålls enligt 1926.
- **19:** 19:21 tvått stöds av 1927; 1926 har tvätt. Den äldre formen rättas därför inte. Kommaskillnader i 19:3,6,8,14,19,22,26 behålls med tryckstöd. 19:9:s komma före och låt tas bort eftersom båda bilderna saknar det. 19:29:s bihustru är ett ord över radgränsen.
- **20:** 20:2 svärdbeväpnade och 20:33 Baal-Tamar stöds av 1926; 1927 har svärdsbeväpnade och Baal-Tammar. 20:28:s komman efter Eleasar och Benjamins barn stöds av 1926. Kommaskillnader i 20:10,13,15,36–38,40,42 behålls enligt 1926. 20:34 och 20:39 har män i båda trycken.
- **21:** 21:5 komme och 21:11 hava stöds av 1926; 1927 har kom respektive utelämnar hava. 21:22:s semikolon efter dem stöds av 1926 (1927: kolon). 21:18:s citatgräns följer 1927. Kommaskillnader i 21:5,8,11,13,16,18,19,21,23 behålls med tryckstöd. Endast Domarbokens avslutning på 369 räknas, ingen Rut-text.

## Kontroller

- Godkänt: `uv run --locked tools/corpus/check_swedish_verification.py`, liggartestet, `tests/swedish_judges_test.py`, svenska kandidatregressioner och sex OSB-metadatatester.
- Godkänt: isolerad C++-byggnad och `ctest --test-dir build/judges -R '^(core-storage-speech|osb-reference)$' --output-on-failure` (2/2).
- Godkänt: 123 resurshashar, databasens integritet och alla 287 korpusteckens täckning i båda Literata-varianterna inklusive reservtypsnitt.
- Exakt före/efter-jämförelse: 168 ändrade `sv1917/Judg`-poster. Alla andra versers text och samtliga koordinater är identiska. Utgåvemappningar, styckegränser, kalenderregler och kanondata är oförändrade. Tidigare liggarposter och fotoevidens bevarade.
- `mjölk- kärlet` → `mjölkkärlet` i 4:19 återför en korrekt SALDO-post och en Dalin-post för mjölkkärl. Nu 12 732 SALDO-sinnen och 7 588 Dalin-poster. Ordlistor, homograffrekvenser, OSB-referensdatabas och resurshashar återbyggda med befintliga verktyg.
- Blankstegskontroll godkänd för övriga filer. Vanlig `git diff --check` rapporterar sex ändrade TSV-raders obligatoriska tomma slutfält som trailing whitespace. Alla ordlisterader har kontrollerats: fyra fält, inga avslutande mellanslag; tabbarna bevarar befintligt TSV-format.
- Ej utfört: separat mänsklig granskning (168 rutor kvar) och fjärr-CI efter publicering. Ingen merge eller stängning av PR har utförts.

Exakt korpusfil före ändringen: `d5073b74a37f9c9e22d51c19ff0c51ea15fcdfa7611064561c97e98fb73984c1`.
Efter återbyggnaden: `38ea22dbe17a292f8bc6d97abfde9624101bc45857034d5a9a59e17761a98753`.
Kapitelvisa texthashar ligger i liggaren och bildhasharna i bildmanifestet.
