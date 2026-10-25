# Sökning efter svenska textfel, 2026-10-25

**Fortsatt arbete:** aktuell kapitel- och boktäckning förs i [den löpande granskningsliggaren](sv1917-verification.md). Siffrorna nedan beskriver tredje omgångens grundläge.

En tredje riktad granskning gav **18 nya bildkontrollerade rättelser: fyra i GT och fjorton i apokryferna**. De är införda som versbundna importpatchar och dokumenterade som R014–R017 och A013–A026 i [erratatabellen](sv1917-errata.md). Ursprungliga nedladdningar är bevarade. Inga rättelser har skickats till Runeberg.

Sökningen gick igenom samtliga 35 515 svenska versposter, totalt 839 500 ordtoken. Bildgranskningen omfattade endast utvalda utdrag. Detta är därför en korpusomfattande kandidatsökning, inte en fullständig OCR-verifiering eller korrekturläsning.

## Är NT bättre korrekturläst än GT och apokryferna?

**Ja, som arbetshypotes för dessa svenska källor. Det är inte en uppmätt rangordning av återstående fel.**

Runebergs [historiska statuslista](https://runeberg.org/bibeln/status.html), läst 2026-10-25 och sidmärkt 2012, anger en avslutad första korrekturläsning för **24 av 27 NT-böcker**, jämfört med **17 av 39 GT-böcker**. Johannes och Apostlagärningarna står som pågående; Första Timoteusbrevet saknar avslutad första läsning. Psaltarens fem delrader har här räknats som en bok; ingen av dem är markerad färdig. Statusen är historisk dokumentation, inte ett löfte om dagens felfrekvens.

Runebergs [inledning](https://runeberg.org/bibeln/) beskriver den kanoniska e-texten som manuellt inskriven 1994–1996, stavningskontrollerad och baserad på flera tryckningar. Våra svenska apokryfer bygger däremot på [1921 års faksimil och dess OCR](https://runeberg.org/apokryf/), med en senare transkription som versskelett. Detta gör olika feltyper sannolika: kanonisk text behöver också sökas efter utelämnade ord och giltiga ord i fel sammanhang; apokryferna ger dessutom tydlig utdelning på bokstavsförväxlingar och trasiga ordgränser.

| Del | Versposter | Ordtoken | Bildverifierade rättelser hittills | Därav denna omgång |
| --- | ---: | ---: | ---: | ---: |
| NT | 7 940 | 180 652 | 1 | 0 |
| GT, utan apokryfer | 23 210 | 552 752 | 16 | 4 |
| Svenska apokryfer | 4 365 | 106 096 | 26 | 14 |

Antalet hittade fel beror också på var vi har letat. Det får inte användas som skattning av fel per ord eller bevis för att NT är färdiggranskat. ”Apokryfer” betyder här de tolv svenska böcker som finns i 1921-importen; resultatet säger inget om kvaliteten i övriga ortodoxa kanonböcker eller våra grekiska och engelska utgåvor.

## Återkörbar sökning

Kör efter korpusbygget:

```sh
uv run --locked tools/corpus/audit_swedish.py
uv run --locked tests/swedish_audit_test.py
```

Skriptet läser korpusdatabasen i skrivskyddat läge och skriver `build/corpus/sv1917-candidates.tsv`. Det gör inga textändringar och hämtar ingenting från nätet. Varje rad har prioritet, korpusdel, vers, ordform, frekvens, sökorsak, eventuella grannord och hela versens sammanhang.

Sökningen kombinerar:

- **Enstaka bokstäver, främmande tecken och inre versaler.** Svenska `I`, `i`, `o`, `å`, `ö`, uttrycket `A och O`, förkortningen `o. s. v.` och `u-farsin` hanteras särskilt. Siffror och textkritiska hakparenteser är inte automatiskt skräp.
- **Sällsynta former nära vanliga korpusord.** Högst fem förekomster, en förväxling av exempelvis `rn/m`, `i/l`, `r/t`, `a/å/ä` eller `o/ö`, och ett grannord som förekommer minst fem gånger och minst fem gånger oftare. Träffar utan lexikonstöd prioriteras före träffar på giltiga lexikonord.
- **Möjliga orddelningar och upprepade bokstäver.** En kort orddel prövas tillsammans med nästa ord mot korpusens ordformer. Tre lika bokstäver flaggas, även om det kan vara en riktig sammansättning.
- **Sällsynta ord utan uppslag.** Former med högst två förekomster som saknas i de befintliga Dalin-, SALDO- och bibellexikonlänkarna hamnar sist. Detta fångar också namn och fullt korrekta äldre böjningar.

Prioritet 1 gäller tecken/bokstavsanomalier, 2 lexikonlösa förväxlingskandidater, 3 orddelningar och förväxlingar som redan är lexikonord, 4 övriga sällsynta former. Prioritet är arbetsordning, inte sannolikhet eller tillstånd att rätta.

Med denna version gav databasen före tredje omgången 5 118 kandidatrader. Efter rättelserna återstår 5 100 rader. Rader kan avse flera fynd i samma vers, och många är korrekta ord. För de två högsta prioriteterna blev resultatet:

| Del | Flaggade verser före → efter | Flaggade verser per 10 000 ord före → efter |
| --- | ---: | ---: |
| NT | 0 → 0 | 0,00 → 0,00 |
| GT | 29 → 25 | 0,52 → 0,45 |
| Apokryfer | 15 → 4 | 1,41 → 0,38 |

Noll starka NT-flaggor betyder endast att dessa regler inte fann något där. Utelämnade ord, ett vanligt ord i fel sammanhang och samma fel upprepat många gånger kan undgå sökningen. GT:s namnlistor och äldre böjningar skapar också fler falska träffar. Apokryfernas lägre antal efter den riktade genomgången får inte tolkas som att de nu är säkrare än GT.

## Vad bildkontrollen lärde oss

Sökningen gav bra kandidater men ibland fel ersättningsförslag:

| Kandidat | Tryckets lydelse | Lärdom |
| --- | --- | --- |
| Syrak 37:3: `har da` | `har du` | Det vanligare grannordet `då` var fel förslag. |
| 1 Mack. 14:42 och 2 Mack. 5:22: `uppsyningsrnan` | `uppsyningsmän` | Både `rn/m` och vokalen behövde granskas; singularen var fel. |
| Ps. 102:27: `förvanda` | `förvandla` | Förslaget `förvända` missade ett bortfallet `l`. |
| Baruk 4:18: `nar` | `har` | Ett riktigt lexikonord kan ändå vara ett transkriptionsfel. Rättelsen tog också bort det ovidkommande SALDO-uppslaget `nar..1`. |
| 2 Mack. 6:17: `avb rrott` | Samma avvikelse i trycket | Bevarat som V002; en OCR-rättning får inte obemärkt bli en rättning av förlagan. |

Två främmande raka citattecken i Syrak 31:16 och 31:25 kunde avlägsnas efter bildkontroll. Däremot är exempelvis `ö` ett vanligt svenskt ord, och `straffförkunnelsen` kan ha tre `f` vid sammansättningsgränsen. En generell teckenstädning skulle förstöra text.

## Mest lovande fortsättning

1. **Jämför den kanoniska e-texten med sidornas OCR.** Koppla bok/kapitel/vers till faksimilsidor, separera spalterna och ta bort noter, rubriker och marginalnummer. Normalisera radbrytningsavstavning för jämförelsen, men behåll originalen. Granska först ordtillägg, bortfall och avvikande ordföljd. Detta kan fånga sådant som en ordlista aldrig ser. Runebergs [korrekturinstruktion](https://runeberg.org/bibeln/0006.html) nämner uttryckligen bortfall av ord och hela meningar.
2. **Gör en oberoende OCR av apokryfspalterna.** En annan OCR-motor eller segmentering kan ge nya ledtrådar där den nuvarande OCR:n och dess avskrifter delar fel. Överensstämmelse mellan två kopior med samma ursprung är inte två oberoende vittnen. Kontrollera avvikelser mot bilden, även när motorerna är överens.
3. **Sök efter giltiga ord i ovanliga fraser.** Fortsätt med små kontextgrupper som `val/väl`, `själ/själv`, `son/som`, `min/mig` och `har/nar`. Samma fras på andra ställen och parallella berättelser kan ge kandidater, men får inte harmoniseras automatiskt.
4. **Skatta kvalitet med ett separat stickprov.** Välj i förväg lika stora ordmängder ur slumpade sidor i NT, GT och apokryferna, spridda över böcker. Läs hela de valda utdragen mot bilden, inklusive de som inte flaggats. Redovisa granskade ord, säkra textfel, tryckvarianter och osäkra fall separat. Först då går det att jämföra fel per ord med meningsfull osäkerhet.

En full ny OCR av alla sidor är inte gjord i denna omgång. Nästa steg med störst förväntad utdelning är apokryfernas oberoende OCR och den kanoniska textens ord-för-ord-differens mot faksimil-OCR, följt av bildkontroll.

## Verifiering

Korpusen, ordlistan, homograferna och OSB-metadata är återbyggda. De 18 ändrade verserna har jämförts med föregående databas: endast de dokumenterade utdragen ändrades; övriga verser och utgåvor är identiska. Kandidatreglernas självkontroll, OSB-metadata-testerna, resurskontrollen och `core-storage-speech` har körts. Resurskontrollens förväntade antal SALDO-poster minskade från 12 749 till 12 748 eftersom det felaktiga `nar` försvann.
