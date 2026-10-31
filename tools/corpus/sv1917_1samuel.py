"""1 Samuel corrections checked against retained EFS 1927 and EFS 1926 images.

See docs/reports/sv1917-1samuel.md and the SAM1 errata rows.
"""
REPLACE = {
    ("1Sam", 1, 1): (' levde en man som hette Elkana,', ' levde en man, som hette Elkana,'),  # SAM1-001
    ("1Sam", 1, 8): ('Då sade hennes man Elkana till henne: »Hanna, varför gråter du? Varför äter du icke? Varför är du så sorgsen? Är jag icke mer för dig än tio söner?', 'Då sade hennes man Elkana till henne: »Hanna, varför gråter du? Varför äter du icke? Varför är du så sorgsen? Är jag icke mer för dig än tio söner?»'),  # SAM1-002
    ("1Sam", 1, 9): ('En gång när de hade ätit och druckit i Silo hände sig,', 'En gång, när de hade ätit och druckit i Silo, hände sig,'),  # SAM1-003
    ("1Sam", 1, 11): (' HERRE Sebaot,', ' »HERRE Sebaot,'),  # SAM1-004
    ("1Sam", 1, 13): (' då trodde Eli att hon var drucken.', ' då trodde Eli, att hon var drucken.'),  # SAM1-005
    ("1Sam", 1, 16): (' ty det är mitt myckna bekymmer och min myckna sorg som har drivit mig att tala ända till denna stund.', ' ty det är mitt myckna bekymmer och min myckna sorg, som har drivit mig att tala ända till denna stund.'),  # SAM1-006
    ("1Sam", 1, 17): (' Israels Gud skall giva dig vad du har utbett dig av honom.', ' Israels Gud skall giva dig, vad du har utbett dig av honom.'),  # SAM1-007
    ("1Sam", 1, 19): (' och HERREN tänkte på henne,', ' och HERREN tänkte på henne.'),  # SAM1-008
    ("1Sam", 1, 24): (' tog hon honom med sig ditupp,', ' tog hon honom med sig ditupp'),  # SAM1-009
    ("1Sam", 1, 26): (' jag är den kvinna som stod här bredvid dig och bad till HERREN.', ' jag är den kvinna, som stod här bredvid dig och bad till HERREN.'),  # SAM1-010
    ("1Sam", 1, 27): (' nu har HERREN givit mig vad jag utbad mig av honom.', ' nu har HERREN givit mig, vad jag utbad mig av honom.'),  # SAM1-011
    ("1Sam", 2, 2): ('Ingen är helig såsom HERREN ty ingen finnes förutom dig;', 'Ingen är helig såsom HERREN, ty ingen finnes förutom dig;'),  # SAM1-012
    ("1Sam", 2, 3): (' Ty HERREN är en Gud som vet allt,', ' Ty HERREN är en Gud, som vet allt,'),  # SAM1-013
    ("1Sam", 2, 5): (' men den moder som fick många barn vissnar bort.', ' men den moder, som fick många barn, vissnar bort.'),  # SAM1-014
    ("1Sam", 2, 10): ('De som strida mot HERREN bliva krossade, ovan dem dundrar han i himmelen; ja, HERREN dömer jordens ändar. Men han giver makt åt sin konung, han upphöjer sin smordes horn.', 'De som strida mot HERREN bliva krossade, ovan dem dundrar han i himmelen; ja, HERREN dömer jordens ändar. Men han giver makt åt sin konung, han upphöjer sin smordes horn.»'),  # SAM1-015
    ("1Sam", 2, 16): (' tag sedan vad dig lyster»', ' tag sedan, vad dig lyster»'),  # SAM1-016
    ("1Sam", 2, 17): ('Och de unga männens synd var så mycket större inför HERREN som folket därigenom lärde sig att förakta HERRENS offer.', 'Och de unga männens synd var så mycket större inför HERREN, som folket därigenom lärde sig att förakta HERRENS offer.'),  # SAM1-017
    ("1Sam", 2, 18): ('Men Samuel gjorde tjänst inför HERRENS ansikte,', 'Men Samuel gjorde tjänst inför HERRENS ansikte'),  # SAM1-018
    ("1Sam", 2, 23): (' allt detta onda som jag hör allt folket här tala om eder?', ' allt detta onda, som jag hör allt folket här tala om eder?'),  # SAM1-019
    ("1Sam", 2, 29): ('Varför förtrampen I då de slaktoffer och spisoffer som jag har påbjudit i min boning?', 'Varför förtrampen I då de slaktoffer och spisoffer, som jag har påbjudit i min boning?'),  # SAM1-020
    ("1Sam", 2, 30): (' Väl har jag sagt att ditt och din faders hus skulle få göra tjänst inför mig evärdligen.', ' Väl har jag sagt, att ditt och din faders hus skulle få göra tjänst inför mig evärdligen.'),  # SAM1-021
    ("1Sam", 2, 35): (' åt honom skall jag bygga ett hus som bliver beståndande,', ' åt honom skall jag bygga ett hus, som bliver beståndande,'),  # SAM1-022
    ("1Sam", 2, 36): ('Och var och en som bliver kvar av ditt hus skall komma och falla ned för honom,', 'Och var och en som bliver kvar av ditt hus skall komma och falla ned för honom'),  # SAM1-023
    ("1Sam", 3, 8): (' Då förstod Eli att det var HERREN som ropade på ynglingen.', '» Då förstod Eli, att det var HERREN, som ropade på ynglingen.'),  # SAM1-024
    ("1Sam", 3, 9): ("Därför sade Eli till Samuel: »Gå och lägg dig; och om han vidare ropar på dig, så säg: 'Tala, HERRE; din tjänare hör.» Och Samuel gick och lade sig på sin plats.", "Därför sade Eli till Samuel: »Gå och lägg dig; och om han vidare ropar på dig, så säg: 'Tala, HERRE; din tjänare hör.'» Och Samuel gick och lade sig på sin plats."),  # SAM1-025
    ("1Sam", 3, 13): ('Ty jag har förkunnat för honom att jag skall vara domare över hans hus till evig tid, därför att han har syndat, i det han visste huru hans söner drogo förbannelse över sig och dock icke höll dem tillbaka.', 'Ty jag har förkunnat för honom, att jag skall vara domare över hans hus till evig tid, därför att han har syndat, i det han visste, huru hans söner drogo förbannelse över sig, och dock icke höll dem tillbaka.'),  # SAM1-026
    ("1Sam", 3, 14): (' vare sig med slaktoffer eller med någon annat offergåva.', ' vare sig med slaktoffer eller med någon annan offergåva.'),  # SAM1-027
    ("1Sam", 3, 15): ('Och Samuel låg kvar ända tills morgonen, då han öppnade dörrarna till HERRENS hus. Och Samuel fruktade för att omtala synen for Eli.', 'Och Samuel låg kvar ända till morgonen, då han öppnade dörrarna till HERRENS hus. Och Samuel fruktade för att omtala synen för Eli.'),  # SAM1-028
    ("1Sam", 3, 17): (' om du döljer for mig något enda ord av det han talade till dig.', ' om du döljer för mig något enda ord av det han talade till dig.'),  # SAM1-029
    ("1Sam", 3, 18): (' han göre vad honom täckes.', ' han göre, vad honom täckes.'),  # SAM1-030
    ("1Sam", 3, 20): (' förstod att Samuel var betrodd att vara HERRENS profet.', ' förstod, att Samuel var betrodd att vara HERRENS profet.'),  # SAM1-031
    ("1Sam", 4, 3): ('När folket kom tillbaka till lägret sade de äldste i Israel:', 'När folket kom tillbaka till lägret, sade de äldste i Israel:'),  # SAM1-033
    ("1Sam", 4, 6): (' Och de fingo veta att HERRENS ark hade kommit in i lägret.', ' Och de fingo veta, att HERRENS ark hade kommit in i lägret.'),  # SAM1-034
    ("1Sam", 4, 8): (' Det var denne Gud som slog egyptierna med alla slags plågor i öknen.', ' Det var denne Gud, som slog egyptierna med alla slags plågor i öknen.'),  # SAM1-035
    ("1Sam", 4, 15): ('Men Eli var nittioåtta år gammal och hans ögon voro starrblinda,', 'Men Eli var nittioåtta år gammal, och hans ögon voro starrblinda,'),  # SAM1-036
    ("1Sam", 4, 20): (' sade kvinnorna som stodo omkring henne:', ' sade kvinnorna, som stodo omkring henne:'),  # SAM1-037
    ("1Sam", 5, 7): ('Då nu invånarna i Asdod sågo att så skedde,', 'Då nu invånarna i Asdod sågo, att så skedde,'),  # SAM1-038
    ("1Sam", 5, 11): ('Och de sände bud och läto för samla alla filistéernas hövdingar och sade:', 'Och de sände bud och läto församla alla filistéernas hövdingar och sade:'),  # SAM1-039
    ("1Sam", 5, 12): ('De av invånarna som icke dogo blevo slagna med bölder;', 'De av invånarna, som icke dogo, blevo slagna med bölder;'),  # SAM1-040
    ("1Sam", 6, 2): (' Låten oss veta på vilket sätt vi skola sända den till dess plats igen.', ' Låten oss veta, på vilket sätt vi skola sända den till dess plats igen.'),  # SAM1-041
    ("1Sam", 6, 3): (' och det skall då också bliva eder kunnigt varför hans hand icke drager sig tillbaka från eder.', ' och det skall då också bliva eder kunnigt, varför hans hand icke drager sig tillbaka från eder.'),  # SAM1-042
    ("1Sam", 6, 7): (' och tagen två kor som giva di,', ' och tagen två kor, som giva di,'),  # SAM1-043
    ("1Sam", 6, 9): (' så var det han som gjorde oss allt detta stora onda; men om så icke sker, då veta vi att det icke var hans hand som hemsökte oss.', ' så var det han, som gjorde oss allt detta stora onda; men om så icke sker, då veta vi, att det icke var hans hand, som hemsökte oss.'),  # SAM1-044
    ("1Sam", 6, 10): (' de togo två kor som gåvo di och spände dem för vagnen;', ' de togo två kor, som gåvo di, och spände dem för vagnen;'),  # SAM1-045
    ("1Sam", 6, 17): ('De svulster av guld som filistéerna gåvo såsom skuldoffer åt HERREN utgjorde:', 'De svulster av guld, som filistéerna gåvo såsom skuldoffer åt HERREN, utgjorde:'),  # SAM1-046
    ("1Sam", 6, 21): ('Och de skickade sändebud till dem som bodde i Kirjat-Jearim och läto säga: »Filistéerna hava sänt tillbaka HERRENS ark; kommen hitned och hämten den upp till eder.', 'Och de skickade sändebud till dem som bodde i Kirjat-Jearim och läto säga: »Filistéerna hava sänt tillbaka HERRENS ark; kommen hitned och hämten den upp till eder.»'),  # SAM1-047
    ("1Sam", 7, 7): ('Men när filistéerna hörde att Israels barn hade församlat sig i Mispa,', 'Men när filistéerna hörde, att Israels barn hade församlat sig i Mispa,'),  # SAM1-048
    ("1Sam", 7, 13): ('Så blevo filistéerna kuvade och kommo icke mer in i Israels land Och HERRENS hand var emot filistéerna,', 'Så blevo filistéerna kuvade och kommo icke mer in i Israels land. Och HERRENS hand var emot filistéerna,'),  # SAM1-049
    ("1Sam", 7, 14): ('Och de städer som filistéerna hade tagit från Israel kommo tillbaka till Israel,', 'Och de städer, som filistéerna hade tagit från Israel, kommo tillbaka till Israel,'),  # SAM1-050
    ("1Sam", 7, 17): (' och där dömde han eljest Israel där byggde han ock ett altare åt HERREN.', ' och där dömde han eljest Israel; där byggde han ock ett altare åt HERREN.'),  # SAM1-051
    ("1Sam", 8, 1): (' satte han sina söner till domare över Israel', ' satte han sina söner till domare över Israel.'),  # SAM1-052
    ("1Sam", 8, 6): (' detta att de sade då: »Giv oss en konung, for att han må döma oss.', ' detta att de sade så: »Giv oss en konung, för att han må döma oss.'),  # SAM1-053
    ("1Sam", 8, 7): (' i det de icke vilja att jag skall vara konung över dem.', ' i det de icke vilja, att jag skall vara konung över dem.'),  # SAM1-054
    ("1Sam", 8, 10): ('Och Samuel sade till folket,', 'Och Samuel sade till folket'),  # SAM1-055
    ("1Sam", 8, 18): ('När I då ropen om hjälp för den konungs skull som I själva haven utvalt åt eder, då skall HERREN icke svara eder.', 'När I då ropen om hjälp för den konungs skull, som I själva haven utvalt åt eder, då skall HERREN icke svara eder.»'),  # SAM1-056
    ("1Sam", 8, 19): ('Men folket ville icke lyssna till Samuels ord, utan sade: »Nej, en konung måste vi hava över oss.»', 'Men folket ville icke lyssna till Samuels ord, utan sade: »Nej, en konung måste vi hava över oss.'),  # SAM1-057
    ("1Sam", 9, 1): ('I Benjamin levde en man som hette Kis,', 'I Benjamin levde en man, som hette Kis,'),  # SAM1-058
    ("1Sam", 9, 2): ('Han hade en son som hette Saul, en ståtlig och fager man; bland Israels barn fanns ingen man som var fagrare än han;', 'Han hade en son, som hette Saul, en ståtlig och fager man; bland Israels barn fanns ingen man, som var fagrare än han;'),  # SAM1-059
    ("1Sam", 9, 5): ('När de så hade kommit in i Sufs land, sade Saul till tjänaren som han hade med sig: »Kom, låt oss gå hem igen; min fader kunde eljest, stället för att tänka på åsninnorna, bliva orolig för vår skull.»', 'När de så hade kommit in i Sufs land, sade Saul till tjänaren som han hade med sig: »Kom, låt oss gå hem igen; min fader kunde eljest, i stället för att tänka på åsninnorna, bliva orolig för vår skull.»'),  # SAM1-060
    ("1Sam", 9, 8): (' för att han må säga oss vilken väg vi böra gå.', ' för att han må säga oss, vilken väg vi böra gå.'),  # SAM1-061
    ("1Sam", 9, 10): (' Så gingo de till staden där gudsmannen fanns.', ' Så gingo de till staden, där gudsmannen fanns.'),  # SAM1-062
    ("1Sam", 9, 11): ('När de nu gingo uppför höjden där staden låg, träffade de några flickor som hade gått ut för att hämta vatten;', 'När de nu gingo uppför höjden, där staden låg, träffade de några flickor, som hade gått ut för att hämta vatten;'),  # SAM1-063
    ("1Sam", 9, 15): ('Men dagen innan Saul kom hade HERREN uppenbarat för Samuel och sagt:', 'Men dagen innan Saul kom, hade HERREN uppenbarat för Samuel och sagt:'),  # SAM1-064
    ("1Sam", 9, 16): (' han skall frälsa mitt folk ifrån filistéernas hand.', ' han skall frälsa mitt folk från filistéernas hand.'),  # SAM1-065
    ("1Sam", 9, 17): ('»Se där är den man om vilken jag sade till dig:', '»Se, där är den man, om vilken jag sade till dig:'),  # SAM1-066
    ("1Sam", 9, 18): ('»Säg mig var siaren bor.', '»Säg mig, var siaren bor.'),  # SAM1-067
    ("1Sam", 9, 23): ('»Giv hit det stycke som jag gav dig, och som jag sade att du skulle förvara hos dig.', '»Giv hit det stycke, som jag gav dig, och som jag sade, att du skulle förvara hos dig.'),  # SAM1-068
    ("1Sam", 9, 24): ('Då tog kocken fram lårstycket med vad därtill hörde, och satte det fram för Saul; och Samuel sade: »Se, här sättes nu fram för dig det som har blivit sparat; ät därav. Ty just för denna stund blev det undanlagt åt dig, då när jag sade att jag hade inbjudit folket.', 'Då tog kocken fram lårstycket med vad därtill hörde och satte det fram för Saul; och Samuel sade: »Se, här sättes nu fram för dig det som har blivit sparat; ät därav. Ty just för denna stund blev det undanlagt åt dig, då när jag sade, att jag hade inbjudit folket.'),  # SAM1-069
    ("1Sam", 9, 27): ('»Säg till tjänaren att han skall gå före oss» – och han fick gå – »men du själv må nu stanna här, så vill jag låta dig höra vad Gud har talat.', '»Säg till tjänaren, att han skall gå före oss» – och han fick gå – »men du själv må nu stanna här, så vill jag låta dig höra, vad Gud har talat.'),  # SAM1-070
    ("1Sam", 10, 2): ("'Åsninnorna som du gick åstad att söka äro återfunna;", "'Åsninnorna som du gick åstad att söka, äro återfunna;"),  # SAM1-071
    ("1Sam", 10, 3): ('Och när du har gått därifrån ett stycke fram och kommit till Tabors terebint skall du där möta tre män som äro på väg upp till Gud i Betel.', 'Och när du har gått därifrån ett stycke fram och kommit till Tabors terebint, skall du där möta tre män, som äro på väg upp till Gud i Betel.'),  # SAM1-072
    ("1Sam", 10, 4): (' och du skall taga emot vad de giva.', ' och du skall taga emot, vad de giva.'),  # SAM1-073
    ("1Sam", 10, 7): ('När du nu ser att dessa tecken inträffa, då må du göra vad tillfället giver vid handen,', 'När du nu ser, att dessa tecken inträffa, då må du göra, vad tillfället giver vid handen,'),  # SAM1-074
    ("1Sam", 10, 8): (' sju dagar skall du vänta till dess jag kommer till dig och förkunnar för dig vad du skall göra.', ' sju dagar skall du vänta, till dess jag kommer till dig och förkunnar för dig vad du skall göra.»'),  # SAM1-075
    ("1Sam", 10, 14): (' Men när vi sågo att de ingenstädes voro att finna,', ' Men när vi sågo, att de ingenstädes voro att finna,'),  # SAM1-076
    ("1Sam", 10, 15): ('»Tala om for mig vad Samuel sade till eder.', '»Tala om för mig, vad Samuel sade till eder.'),  # SAM1-077
    ("1Sam", 10, 16): ('»Han omtalade för oss att åsninnorna voro återfunna.', '»Han omtalade för oss, att åsninnorna voro återfunna.'),  # SAM1-078
    ("1Sam", 10, 17): ('Därefter kallade Samuel folket tillsammans till HERREN,', 'Därefter kallade Samuel folket tillsammans till HERREN'),  # SAM1-079
    ("1Sam", 10, 20): (' då träffades Benjamin stam av lotten.', ' då träffades Benjamins stam av lotten.'),  # SAM1-080
    ("1Sam", 10, 22): ('Då frågade de HERREN ännu en gång: »Har någon mer kommit hit? HERREN svarade: »Han har gömt sig bland trossen.»', 'Då frågade de HERREN ännu en gång: »Har någon mer kommit hit?» HERREN svarade: »Han har gömt sig bland trossen.»'),  # SAM1-081
    ("1Sam", 10, 23): ('Då skyndade de dit och hämtad honom därifrån,', 'Då skyndade de dit och hämtade honom därifrån,'),  # SAM1-082
    ("1Sam", 10, 27): (' Men han låtsade som om han icke märkte det.', ' Men han låtsade, som om han icke märkte det.'),  # SAM1-083
    ("1Sam", 11, 5): (' Och de förtäljde för honom vad mannen från Jabes hade sagt.', ' Och de förtäljde för honom, vad männen från Jabes hade sagt.'),  # SAM1-084
    ("1Sam", 11, 10): (' och I mån då göra med oss vadhelst I finnen för gott.', ' och I mån då göra med oss, vadhelst I finnen för gott.'),  # SAM1-085
    ("1Sam", 11, 11): (' och de trängde in i lägret vid morgonväkten och nedgjorde ammoniterna,', ' och de trängde in i lägret vid morgonväkten och nedgjorde ammoniterna'),  # SAM1-086
    ("1Sam", 12, 2): ('Nu är det eder konung som skall vara eder ledare, nu då jag är gammal och grå; I haven ju redan mina söner ibland eder. Hittills är det jag som har varit eder ledare,', 'Nu är det eder konung, som skall vara eder ledare, nu då jag är gammal och grå; I haven ju redan mina söner ibland eder. Hittills är det jag, som har varit eder ledare,'),  # SAM1-087
    ("1Sam", 12, 3): ('Se har står jag,', 'Se, här står jag,'),  # SAM1-088
    ("1Sam", 12, 8): ('När Jakob hade kommit fram till Egypten,', 'När Jakob hade kommit till Egypten,'),  # SAM1-089
    ("1Sam", 12, 12): ('Men när I sågen att Nahas,', 'Men när I sågen, att Nahas,'),  # SAM1-090
    ("1Sam", 12, 14): (' både I och den konung som regerar över eder mån följa HERREN,', ' både I och den konung, som regerar över eder, mån följa HERREN,'),  # SAM1-091
    ("1Sam", 12, 16): ('Träden nu ock fram och sen det stora under som HERREN skall göra inför edra ögon.', 'Träden nu ock fram och sen det stora under, som HERREN skall göra inför edra ögon.'),  # SAM1-092
    ("1Sam", 12, 17): (' men jag vill ropa till HERREN att han må låta det dundra och regna. Så skolen I märka och se huru mycket ont I haven gjort i HERRENS ögon genom eder begäran att få en konung.', ' men jag vill ropa till HERREN, att han må låta det dundra och regna. Så skolen I märka och se, huru mycket ont I haven gjort i HERRENS ögon genom eder begäran att få en konung.'),  # SAM1-093
    ("1Sam", 12, 19): (' eftersom vi till alla våra andra synder ock hava lagt det onda att vi hava begärt att få en konung.', ' eftersom vi till alla våra andra synder ock hava lagt det onda, att vi hava begärt att få en konung.'),  # SAM1-094
    ("1Sam", 12, 24): (' Ty sen vilka stora ting han har gjort med eder!', ' Ty sen, vilka stora ting han har gjort med eder!'),  # SAM1-095
    ("1Sam", 13, 4): ('Så fick hela Israel höra omtalas att Saul hade dräpt filistéernas fogde,', 'Så fick hela Israel höra omtalas, att Saul hade dräpt filistéernas fogde,'),  # SAM1-096
    ("1Sam", 13, 5): (' trettio tusen vagnar och sex tusen ryttare,', ' trettio tusen vagnar och sex tusen ryttare'),  # SAM1-097
    ("1Sam", 13, 8): (' intill den tid Samuel hade bestämt men Samuel likväl icke kom till Gilgal,', ' intill den tid Samuel hade bestämt, men Samuel likväl icke kom till Gilgal,'),  # SAM1-098
    ("1Sam", 13, 11): ('»När jag såg att folket skingrade sig och gick ifrån mig,', '»När jag såg, att folket skingrade sig och gick ifrån mig,'),  # SAM1-099
    ("1Sam", 13, 15): (' Men Saul mönstrade det folk som fanns hos honom:', ' Men Saul mönstrade det folk, som fanns hos honom:'),  # SAM1-100
    ("1Sam", 13, 21): ('när det hade blivit något fel med eggen på skärorna eller plogbillarna,', 'när det hade blivit något fel med eggen på skärorna eller plogbillarna'),  # SAM1-101
    ("1Sam", 14, 1): ('Så hände sig nu en dag att Jonatan,', 'Så hände sig nu en dag, att Jonatan,'),  # SAM1-102
    ("1Sam", 14, 2): (' och folket som han hade hos sig utgjorde vid pass sex hundra man;', ' och folket, som han hade hos sig, utgjorde vid pass sex hundra man;'),  # SAM1-103
    ("1Sam", 14, 3): (' har då efoden.', ' bar då efoden.'),  # SAM1-104
    ("1Sam", 14, 6): (' låt oss gå över till dessa oomskurnas utpost,', ' låt oss gå över till dessa oomskurnas utpost;'),  # SAM1-105
    ("1Sam", 14, 7): (' jag följer dig vart du vill.', ' jag följer dig, vart du vill.'),  # SAM1-106
    ("1Sam", 14, 11): (' hebréerna krypa ut ur hålen där de hava gömt sig.', ' hebréerna krypa ut ur hålen, där de hava gömt sig.'),  # SAM1-107
    ("1Sam", 14, 14): ('I det första anfallet nedgjorde så Jonatan och hans vapendragare vid pass tjugu män,', 'I det första anfallet nedgjorde så Jonatan och hans vapendragare vid pass tjugu män'),  # SAM1-108
    ("1Sam", 14, 16): ('Och Sauls väktare i Gibea i Benjamin fingo se att hopen var i upplösning,', 'Och Sauls väktare i Gibea i Benjamin fingo se, att hopen var i upplösning,'),  # SAM1-109
    ("1Sam", 14, 17): (' funno de att Jonatan och hans vapendragare icke voro där.', ' funno de, att Jonatan och hans vapendragare icke voro där.'),  # SAM1-110
    ("1Sam", 14, 20): ('Och Saul och allt det folk som han hade hos sig församlade sig och drogo till stridsplatsen; där fingo de se att den ene hade lyft sitt svärd mot den andre,', 'Och Saul och allt det folk, som han hade hos sig, församlade sig och drogo till stridsplatsen; där fingo de se, att den ene hade lyft sitt svärd mot den andre,'),  # SAM1-111
    ("1Sam", 14, 21): ('Och de hebréer som sedan gammalt lydde under filistéerna,', 'Och de hebréer, som sedan gammalt lydde under filistéerna,'),  # SAM1-112
    ("1Sam", 14, 22): ('Och när de israeliter som hade gömt sig i Efraims bergsbygd hörde att filistéerna flydde,', 'Och när de israeliter, som hade gömt sig i Efraims bergsbygd, hörde, att filistéerna flydde,'),  # SAM1-113
    ("1Sam", 14, 24): ('»Förbannad vare den man som förtär någon föda före aftonen,', '»Förbannad vare den man, som förtär någon föda före aftonen,'),  # SAM1-114
    ("1Sam", 14, 27): (' därför räckte han ut staven som han hade i sin hand och doppade dess ända i honungskakan,', ' därför räckte han ut staven, som han hade i sin hand, och doppade dess ända i honungskakan,'),  # SAM1-115
    ("1Sam", 14, 28): ("'Förbannad vare den man som dag förtär någon föda.", "'Förbannad vare den man, som i dag förtär någon föda."),  # SAM1-116
    ("1Sam", 14, 35): (' detta var det första altare som han byggde åt HERREN.', ' detta var det första altare, som han byggde åt HERREN.'),  # SAM1-117
    ("1Sam", 14, 38): (' för att I mån få veta och se vari den synd består,', ' för att I mån få veta och se, vari den synd består,'),  # SAM1-118
    ("1Sam", 14, 43): ('»Omtala för mig vad du har gjort.', '»Omtala för mig, vad du har gjort.'),  # SAM1-119
    ("1Sam", 14, 52): (' Och varhelst Saul såg någon rask och krigsduglig man tog han honom i sin tjänst.', ' Och varhelst Saul såg någon rask och krigsduglig man, tog han honom i sin tjänst.'),  # SAM1-120
    ("1Sam", 15, 3): ('Så drag nu åstad och slå amalekiterna och giv dem till spillo,', 'Så drag nu åstad och slå amalekiterna och giv dem till spillo'),  # SAM1-121
    ("1Sam", 15, 11): ('»Jag ångrar att jag har gjort Saul till konung,', '»Jag ångrar, att jag har gjort Saul till konung,'),  # SAM1-122
    ("1Sam", 15, 12): (' Då blev det berättat för Samuel att Saul hade kommit till Karmel och där rest åt sig en minnesstod,', ' Då blev det berättat för Samuel, att Saul hade kommit till Karmel och där rest åt sig en minnesstod,'),  # SAM1-123
    ("1Sam", 15, 14): ('»Vad är det då för ett läte av får som ljuder i mina öron, och vad är det för ett läte av fäkreatur som jag hör?', '»Vad är det då för ett läte av får, som ljuder i mina öron, och vad är det för ett läte av fäkreatur, som jag hör?'),  # SAM1-124
    ("1Sam", 15, 16): (' så vill jag förkunna för dig vad HERREN i natt har talat till mig.', ' så vill jag förkunna för dig, vad HERREN i natt har talat till mig.»'),  # SAM1-125
    ("1Sam", 15, 21): (' för att offra det åt HERREN din Gud,', ' för att offra det åt HERREN, din Gud,'),  # SAM1-126
    ("1Sam", 15, 33): ('»Såsom ditt svärd har gjort kvinnor barnlösa så skall ock din moder bliva barnlös framför andra kvinnor.', '»Såsom ditt svärd har gjort kvinnor barnlösa, så skall ock din moder bliva barnlös framför andra kvinnor.'),  # SAM1-127
    ("1Sam", 15, 35): ('Och Samuel ville icke mer se Saul så länge han levde, ty Samuel sörjde över Saul, eftersom HERREN ångrade att han hade gjort Saul till konung över Israel.', 'Och Samuel ville icke mer se Saul, så länge han levde, ty Samuel sörjde över Saul, eftersom HERREN ångrade, att han hade gjort Saul till konung över Israel.'),  # SAM1-128
    ("1Sam", 16, 1): (' ty jag vill icke längre att han skall vara konung över Israel. Fyll ditt horn med olja och gå åstad jag vill sända dig till betlehemiten Isai,', ' ty jag vill icke längre, att han skall vara konung över Israel. Fyll ditt horn med olja och gå åstad, jag vill sända dig till betlehemiten Isai,'),  # SAM1-129
    ("1Sam", 16, 3): (' och jag skall då själv låta dig veta vad du bör göra,', ' och jag skall då själv låta dig veta, vad du bör göra,'),  # SAM1-130
    ("1Sam", 16, 4): ('Samuel gjorde vad HERREN hade sagt, och kom så till Bet-Lehem Men när de äldste i staden fingo se honom,', 'Samuel gjorde, vad HERREN hade sagt, och kom så till Bet-Lehem. Men när de äldste i staden fingo se honom,'),  # SAM1-131
    ("1Sam", 16, 7): ('Men HERREN sade till Samuel »Skåda icke på hans utseende och på hans högväxta gestalt, ty jag har förkastat honom. Ty det är icke såsom en människa ser; en människa ser på det som är för ögonen men HERREN ser till hjärtat.', 'Men HERREN sade till Samuel: »Skåda icke på hans utseende och på hans högväxta gestalt, ty jag har förkastat honom. Ty det är icke såsom en människa ser; en människa ser på det som är för ögonen, men HERREN ser till hjärtat.'),  # SAM1-132
    ("1Sam", 16, 12): ('Då sände han åstad och lät hämta honom,', 'Då sände han åstad och lät hämta honom;'),  # SAM1-133
    ("1Sam", 16, 16): (' att de söka upp en man som är kunnig i harpospel,', ' att de söka upp en man, som är kunnig i harpospel,'),  # SAM1-134
    ("1Sam", 16, 18): ('»Betlehemiten Isai har en son som jag har funnit vara kunnig i strängaspel,', '»Betlehemiten Isai har en son, som jag har funnit vara kunnig i strängaspel,'),  # SAM1-135
    ("1Sam", 17, 4): ('Då framträdde ur filistéernas skaror en envigeskämpe vid namn Goljat,', 'Då framträdde ur filistéernas skaror en envigeskämpe vid namn Goljat'),  # SAM1-136
    ("1Sam", 17, 8): (' väljen nu ut åt eder en man som må komma hitned till mig.', ' väljen nu ut åt eder en man, som må komma hitned till mig.'),  # SAM1-137
    ("1Sam", 17, 10): ('Och filistéen sade ytterligare: »Jag har i dag smädat Israels här. Skaffen nu hit någon, så att vi få strida med varandra!', 'Och filistéen sade ytterligare: »Jag har i dag smädat Israels här. Skaffen nu hit någon, så att vi få strida med varandra!»'),  # SAM1-138
    ("1Sam", 17, 24): (' när de fingo se honom och fruktade storligen.', ' när de fingo se honom, och fruktade storligen.'),  # SAM1-139
    ("1Sam", 17, 25): (' Men den man som slår ned honom vill konungen begåva med stor rikedom,', ' Men den man, som slår ned honom, vill konungen begåva med stor rikedom,'),  # SAM1-140
    ("1Sam", 17, 26): ('Och David sade till de man som stodo bredvid honom: »Vad får den man som slår ned denne filisté och därmed tager bort sådan smälek från Israel?', 'Och David sade till de män, som stodo bredvid honom: »Vad får den man, som slår ned denne filisté och därmed tager bort sådan smälek från Israel?'),  # SAM1-141
    ("1Sam", 17, 27): ('»Detta får den man som slår ned honom.', '»Detta får den man, som slår ned honom.'),  # SAM1-142
    ("1Sam", 17, 28): (' hörde huru han talade med männen;', ' hörde, huru han talade med männen;'),  # SAM1-143
    ("1Sam", 17, 33): (' du är du ju allenast en yngling,', ' du är ju allenast en yngling,'),  # SAM1-144
    ("1Sam", 17, 36): (' så skall det gå denne oomskurne filisté såsom det gick vart och ett av dessa djur,', ' så skall det gå denne oomskurne filisté, såsom det gick vart och ett av dessa djur,'),  # SAM1-145
    ("1Sam", 17, 41): (' och hans sköld bärare gick framför honom.', ' och hans sköldbärare gick framför honom.'),  # SAM1-146
    ("1Sam", 17, 43): ('»Menar du att jag är en hund,', '»Menar du, att jag är en hund,'),  # SAM1-147
    ("1Sam", 17, 46): (' så skola alla länder förnimma att Israel har en Gud.', ' så skola alla länder förnimma, att Israel har en Gud.'),  # SAM1-148
    ("1Sam", 17, 47): ('Och hela denna hop skall förnimma att det icke är genom svärd och spjut som HERREN giver seger;', 'Och hela denna hop skall förnimma, att det icke är genom svärd och spjut som HERREN giver seger;'),  # SAM1-149
    ("1Sam", 17, 51): (' När filistéerna nu sågo att deras kämpe var död,', ' När filistéerna nu sågo, att deras kämpe var död,'),  # SAM1-150
    ("1Sam", 17, 52): ('Men Israels och Juda män stodo upp och höjde ett härskri och förföljde filistéerna ända dit där vägen går till Gai, och ända intill Ekrons portar; och filistéer föllo och lågo slagna på vägen till Saaraim,', 'Men Israels och Juda män stodo upp och höjde ett härskri och förföljde filistéerna ända dit, där vägen går till Gai, och ända intill Ekrons portar; och filistéer föllo och lågo slagna på vägen till Saaraim'),  # SAM1-151
    ("1Sam", 17, 58): (' yngling»', ' yngling?»'),  # SAM1-152
    ("1Sam", 18, 4): ('Och Jonatan tog av sig manteln som han hade på sig och gav den åt David, så ock sina övriga kläder, ända till sitt svärd, sin båge och sitt bälte', 'Och Jonatan tog av sig manteln, som han hade på sig, och gav den åt David, så ock sina övriga kläder, ända till sitt svärd, sin båge och sitt bälte.'),  # SAM1-153
    ("1Sam", 18, 5): (' hade han framgång överallt dit Saul sände honom;', ' hade han framgång överallt, dit Saul sände honom;'),  # SAM1-154
    ("1Sam", 18, 15): ('Då nu Saul såg att han hade så stor framgång,', 'Då nu Saul såg, att han hade så stor framgång,'),  # SAM1-155
    ("1Sam", 18, 19): ('När tiden kom att Sauls dotter Merab skulle hava givits åt David,', 'När tiden kom, att Sauls dotter Merab skulle hava givits åt David,'),  # SAM1-156
    ("1Sam", 18, 22): ('Och Saul bjöd sina tjänare att de hemligen skulle tala så med David:', 'Och Saul bjöd sina tjänare, att de hemligen skulle tala så med David:'),  # SAM1-157
    ("1Sam", 18, 25): ('Då tillsade Saul dem att de skulle säga så till David: »Konungen begär ingen annan brudgåva än förhudarna av ett hundra filistéer, för att hämnd så må tagas på konungens fiender.» Saul hoppades nämligen att han skulle få David fälld genom filistéernas hand.', 'Då tillsade Saul dem, att de skulle säga så till David: »Konungen begär ingen annan brudgåva än förhudarna av ett hundra filistéer, för att hämnd så må tagas på konungens fiender.» Saul hoppades nämligen, att han skulle få David fälld genom filistéernas hand.'),  # SAM1-158
    ("1Sam", 18, 26): ('När så hans tjänare omtalade för David vad han hade sagt,', 'När så hans tjänare omtalade för David, vad han hade sagt,'),  # SAM1-159
    ("1Sam", 18, 28): ('Men Saul såg och förstod att HERREN var med David; Och Sauls dotter Mikal hade honom kär.', 'Men Saul såg och förstod, att HERREN var med David; och Sauls dotter Mikal hade honom kär.'),  # SAM1-160
    ("1Sam", 19, 2): (' Tag dig alltså till vara i morgon och håll dig gömd på någon plats där du kan vara dold.', ' Tag dig alltså till vara i morgon och håll dig gömd på någon plats, där du kan vara dold.'),  # SAM1-161
    ("1Sam", 19, 3): ('Men själv vill jag gå ut och ställa mig bredvid min fader på marken, där du är, och jag vill tala om dig med min fader; om jag då märker något, skall jag sedan omtala det för dig.', 'Men själv vill jag gå ut och ställa mig bredvid min fader på marken, där du är, och jag vill tala om dig med min fader; om jag då märker något, skall jag sedan omtala det för dig.»'),  # SAM1-162
    ("1Sam", 19, 4): ('»Konungen må icke försynda sig på sin tjänare David ty han har icke försyndat sig mot dig,', '»Konungen må icke försynda sig på sin tjänare David, ty han har icke försyndat sig mot dig,'),  # SAM1-163
    ("1Sam", 19, 16): (' fingo de se att det var husguden som låg i sängen,', ' fingo de se, att det var husguden, som låg i sängen,'),  # SAM1-164
    ("1Sam", 19, 19): ('Och det blev berättat för Saul att David var i Najot vid Rama.', 'Och det blev berättat för Saul, att David var i Najot vid Rama.'),  # SAM1-165
    ("1Sam", 19, 20): (' Men när Sauls utskickade fingo se skaran av profeterna i profetisk hänryckning,', ' Men när Sauls utskickade fingo se skaran av profeterna i profetisk hänryckning'),  # SAM1-166
    ("1Sam", 20, 1): ('Men David flydde från Najot vid Rama och kom till Jonatan och sade:', 'Men David flydde ifrån Najot vid Rama och kom till Jonatan och sade:'),  # SAM1-167
    ("1Sam", 20, 3): ('»Din fader vet väl att jag har funnit nåd för dina ögon;', '»Din fader vet väl, att jag har funnit nåd för dina ögon;'),  # SAM1-168
    ("1Sam", 20, 7): (' så märker du därav att han har beslutit min ofärd.', ' så märker du därav, att han har beslutit min ofärd.'),  # SAM1-169
    ("1Sam", 20, 9): (' Om jag märker att min fader har beslutit att låta ofärd komma över dig,', ' Om jag märker, att min fader har beslutit att låta ofärd komma över dig,'),  # SAM1-170
    ("1Sam", 20, 10): ('»Vem skall omtala för mig detta, eller säga mig om din fader giver dig ett hårt svar?', '»Vem skall omtala för mig detta eller säga mig, om din fader giver dig ett hårt svar?'),  # SAM1-171
    ("1Sam", 20, 12): ('»Vid HERREN» Israels Gud: om jag finner att det låter gott för David,', '»Vid HERREN, Israels Gud: om jag finner, att det låter gott för David,'),  # SAM1-172
    ("1Sam", 20, 17): (' ty han hade honom lika kär som han hade sitt eget liv;', ' ty han hade honom lika kär, som han hade sitt eget liv;'),  # SAM1-173
    ("1Sam", 20, 19): ('Men gå i övermorgon skyndsamt ned till den plats där du gömde dig den dag då ogärningen skulle hava skett,', 'Men gå i övermorgon skyndsamt ned till den plats, där du gömde dig den dag, då ogärningen skulle hava skett,'),  # SAM1-174
    ("1Sam", 20, 21): ('»Se,', "'Se,"),  # SAM1-175
    ("1Sam", 20, 30): ('»Du son till en otuktig kvinna! Visste jag då icke att du hade funnit behag i Isais son,', '»Du, son till en otuktig kvinna! Visste jag då icke, att du hade funnit behag i Isais son,'),  # SAM1-176
    ("1Sam", 20, 33): (' och nu märkte Jonatan att hans fader hade beslutit att döda David.', ' och nu märkte Jonatan, att hans fader hade beslutit att döda David.'),  # SAM1-177
    ("1Sam", 20, 36): ('»Spring och sök reda på pilarna som jag skjuter av.', '»Spring och sök reda på pilarna, som jag skjuter av.'),  # SAM1-178
    ("1Sam", 20, 37): ('Och när gossen kom till det ställe dit Jonatan hade avskjutit pilen,', 'Och när gossen kom till det ställe, dit Jonatan hade avskjutit pilen,'),  # SAM1-179
    ("1Sam", 20, 38): (' Och gossen som Jonatan hade med sig tog upp pilen och kom till sin herre.', ' Och gossen, som Jonatan hade med sig, tog upp pilen och kom till sin herre.'),  # SAM1-180
    ("1Sam", 20, 39): ('Men gossen visste icke varom fråga var;', 'Men gossen visste icke, varom fråga var;'),  # SAM1-181
    ("1Sam", 21, 2): ("'Ingen får veta något om det uppdrag vari jag sänder dig,", "'Ingen får veta något om det uppdrag, vari jag sänder dig,"),  # SAM1-182
    ("1Sam", 21, 4): ('Prästen svarade David och sade ', 'Prästen svarade David och sade: '),  # SAM1-183
    ("1Sam", 21, 9): (' det svärd som har tillhört filistéen Goljat,', ' det svärd, som har tillhört filistéen Goljat,'),  # SAM1-184
    ("1Sam", 21, 11): ("Men Akis' tjänare sade till honom: »Detta är ju David, landets konung! Det är ju till dennes ära man sjunger så under dansen: 'Saul har slagit sina tusen, men David sina tio tusen.'", "Men Akis' tjänare sade till honom: »Detta är ju David, landets konung! Det är ju till dennes ära man sjunger så under dansen: 'Saul har slagit sina tusen, men David sina tio tusen.'»"),  # SAM1-185
    ("1Sam", 21, 14): ('»I sen ju huru vanvettigt mannen beter sig.', '»I sen ju, huru vanvettigt mannen beter sig.'),  # SAM1-186
    ("1Sam", 22, 2): ('Och till honom församlade sig alla slags män som voro i något trångmål,', 'Och till honom församlade sig alla slags män, som voro i något trångmål,'),  # SAM1-187
    ("1Sam", 22, 3): ('»Låt min fader och min moder få komma hitöver och vara hos eder till dess jag får veta vad Gud vill göra med mig.', '»Låt min fader och min moder få komma hitöver och vara hos eder, till dess jag får veta, vad Gud vill göra med mig.'),  # SAM1-188
    ("1Sam", 22, 6): ('Och Saul fick höra att man hade fått spaning på David och de män som voro med honom.', 'Och Saul fick höra, att man hade fått spaning på David och de män som voro med honom.'),  # SAM1-189
    ("1Sam", 22, 8): (' och ingen har uppenbarat för mig att min son har slutit förbund med Isais son.', ' och ingen har uppenbarat för mig, att min son har slutit förbund med Isais son.'),  # SAM1-190
    ("1Sam", 22, 17): (' och fastän de visste att han flydde,', ' och fastän de visste, att han flydde,'),  # SAM1-191
    ("1Sam", 22, 18): (' Edoméen Doeg trädde då fram och stötte ned prästerna och dödade på den dagen åttiofem män som buro linne-efod.', ' Edoméen Doeg trädde då fram och stötte ned prästerna och dödade på den dagen åttiofem män, som buro linne-efod.'),  # SAM1-192
    ("1Sam", 22, 21): ('Och Ebjatar omtalade för David att Saul hade dräpt HERRENS präster.', 'Och Ebjatar omtalade för David, att Saul hade dräpt HERRENS präster.'),  # SAM1-193
    ("1Sam", 22, 22): ('»Jag förstod redan då att edoméen Doeg, eftersom han var där, skulle omtala allt för Saul. Det är jag som är orsaken till att hela din faders hus har förgåtts.', '»Jag förstod redan då, att edoméen Doeg, eftersom han var där, skulle omtala allt för Saul. Det är jag, som är orsaken till att hela din faders hus har förgåtts.'),  # SAM1-194
    ("1Sam", 23, 3): (' Och nu skulla vi därtill draga åstad till Kegila,', ' Och nu skulle vi därtill draga åstad till Kegila'),  # SAM1-195
    ("1Sam", 23, 7): ('Och det blev berättat för Saul att David hade dragit in i Kegila.', 'Och det blev berättat för Saul, att David hade dragit in i Kegila.'),  # SAM1-196
    ("1Sam", 23, 8): ('Därefter bådade Saul upp allt folket till strid, för att draga ned till Kegila och där innesluta David och hans man.', 'Därefter bådade Saul upp allt folket till strid för att draga ned till Kegila och där innesluta David och hans män.'),  # SAM1-197
    ("1Sam", 23, 9): ('Men när David fick veta att Saul stämplade ont mot honom,', 'Men när David fick veta, att Saul stämplade ont mot honom,'),  # SAM1-198
    ("1Sam", 23, 10): (' din tjänare har hört att Saul har i sinnet att komma mot Kegila och fördärva staden för min skull.', ' din tjänare har hört, att Saul har i sinnet att komma mot Kegila och fördärva staden för min skull.'),  # SAM1-199
    ("1Sam", 23, 12): ('»Skola Kegilas borgare då utlämna mig och mina man åt Saul?', '»Skola Kegilas borgare då utlämna mig och mina män åt Saul?'),  # SAM1-200
    ("1Sam", 23, 13): (' När det då blev berättat för Saul att David hade flytt undan från Kegila avstod han från att draga ut.', ' När det då blev berättat för Saul, att David hade flytt undan från Kegila, avstod han från att draga ut.'),  # SAM1-201
    ("1Sam", 23, 15): (' förnam han att Saul hade dragit ut för att söka döda honom.', ' förnam han, att Saul hade dragit ut för att söka döda honom.'),  # SAM1-202
    ("1Sam", 23, 22): (' ty man har sagt mig att han är mycket listig.', ' ty man har sagt mig, att han är mycket listig.'),  # SAM1-203
    ("1Sam", 23, 23): ('Och sen efter och tagen reda på alla gömställen där han kan gömma sig;', 'Och sen efter och tagen reda på alla gömställen, där han kan gömma sig;'),  # SAM1-204
    ("1Sam", 23, 25): (' om talade man det för denne,', ' omtalade man det för denne,'),  # SAM1-205
    ("1Sam", 23, 26): ('Och Saul gick på ena sidan om berget, och David med sina män på andra sidan. Men just som David var stadd på flykt för att komma undan Saul, under det att Saul och hans män sökte kringränna David och hans män för att taga dem till fånga', 'Och Saul gick på ena sidan om berget och David med sina män på andra sidan. Men just som David var stadd på flykt för att komma undan Saul, under det att Saul och hans män sökte kringränna David och hans män för att taga dem till fånga,'),  # SAM1-206
    ("1Sam", 24, 2): (' omtalade man för honom att David var i En-Gedis öken.', ' omtalade man för honom, att David var i En-Gedis öken.'),  # SAM1-207
    ("1Sam", 24, 5): (' detta är den dag om vilken HERREN har sagt till dig:', ' detta är den dag, om vilken HERREN har sagt till dig:'),  # SAM1-208
    ("1Sam", 24, 7): ('»HERREN låte det vara fjärran ifrån mig att jag skulle göra detta mot min herre, mot HERRENS smorde, att jag skulle uträcka min hand mot honom; han är ju HERRENS Smorde.', '»HERREN låte det vara fjärran ifrån mig, att jag skulle göra detta mot min herre, mot HERRENS smorde, att jag skulle uträcka min hand mot honom; han är ju HERRENS smorde.'),  # SAM1-209
    ("1Sam", 24, 10): (' som säga att David söker din ofärd?', ' som säga, att David söker din ofärd?'),  # SAM1-210
    ("1Sam", 24, 11): ("Du har ju i dag med egna ögon sett hurusom jag skonade dig, när HERREN i dag hade givit dig i min hand i grottan Och man uppmanade mig att dräpa dig; jag tänkte: 'Jag vill icke uträcka min hand mot min herre; han är ju HERRENS Smorde.", "Du har ju i dag med egna ögon sett, hurusom jag skonade dig, när HERREN i dag hade givit dig i min hand i grottan och man uppmanade mig att dräpa dig; jag tänkte: 'Jag vill icke uträcka min hand mot min herre; han är ju HERRENS smorde.'"),  # SAM1-211
    ("1Sam", 24, 12): (' må du märka och se att jag icke har velat göra något ont eller begå någon förbrytelse,', ' må du märka och se, att jag icke har velat göra något ont eller begå någon förbrytelse,'),  # SAM1-212
    ("1Sam", 24, 14): (' därför skall hand icke röra dig.»', ' därför skall min hand icke röra dig.'),  # SAM1-213
    ("1Sam", 24, 21): ('Och nu vet jag väl att du skall bliva konung,', 'Och nu vet jag väl, att du skall bliva konung,'),  # SAM1-214
    ("1Sam", 24, 22): ('Men lova mig nu med ed vid HERREN att du icke utrotar mina avkomlingar efter mig och icke utplånar mitt namn ur min faders hus.', 'Men lova mig nu med ed vid HERREN, att du icke utrotar mina avkomlingar efter mig och icke utplånar mitt namn ur min faders hus.'),  # SAM1-215
    ("1Sam", 25, 2): ('I Maon fanns då en man som hade sin boskapsskötsel i Karmel,', 'I Maon fanns då en man, som hade sin boskapsskötsel i Karmel,'),  # SAM1-216
    ("1Sam", 25, 4): ('När nu David i öknen fick höra att Nabal klippte sina får,', 'När nu David i öknen fick höra, att Nabal klippte sina får,'),  # SAM1-217
    ("1Sam", 25, 6): ('»Frid vare med dig själv frid vare med ditt hus,', "'Frid vare med dig själv, frid vare med ditt hus,"),  # SAM1-218
    ("1Sam", 25, 7): ('Jag har nu hört att du håller på med fårklippning. Nu är det så att dina herdar hava vistats i vårt grannskap,', 'Jag har nu hört, att du håller på med fårklippning. Nu är det så, att dina herdar hava vistats i vårt grannskap,'),  # SAM1-219
    ("1Sam", 25, 8): ('Fråga dina tjänare därom, så skola de själva säga dig det. Låt nu våra män finna nåd för dina ögon. Vi hava ju kommit hit på en glad dag. Giv därför åt dina tjänare och åt din son David vad du kan hava till hands.»', "Fråga dina tjänare därom, så skola de själva säga dig det. Låt nu våra män finna nåd för dina ögon. Vi hava ju kommit hit på en glad dag. Giv därför åt dina tjänare och åt din son David vad du kan hava till hands.'»"),  # SAM1-220
    ("1Sam", 25, 9): (' talade de på Davids vägnar till Nabal alldeles såsom det var dem befallt,', ' talade de på Davids vägnar till Nabal, alldeles såsom det var dem befallt,'),  # SAM1-221
    ("1Sam", 25, 10): (' I denna tid är det många tjänare som rymma från sina herrar.', ' I denna tid är det många tjänare, som rymma från sina herrar.'),  # SAM1-222
    ("1Sam", 25, 11): (' och giva detta åt män om vilka jag icke ens vet varifrån de äro?', ' och giva detta åt män, om vilka jag icke ens vet, varifrån de äro?'),  # SAM1-223
    ("1Sam", 25, 12): (' berättade de for honom allt,', ' berättade de för honom allt,'),  # SAM1-224
    ("1Sam", 25, 18): (' ett hundra russinkakor och två hundra fikonkakor,', ' ett hundra russinkakor och två hundra fikonkakor'),  # SAM1-225
    ("1Sam", 25, 21): (' men har har vedergällt mig med ont för gott.', ' men han har vedergällt mig med ont för gott.'),  # SAM1-226
    ("1Sam", 25, 24): (' Men låt din tjänarinna få tala inför dig,', ' Men låt din tjänarinna få tala inför dig'),  # SAM1-227
    ("1Sam", 25, 25): (' har icke sett de män som du,', ' har icke sett de män, som du,'),  # SAM1-228
    ("1Sam", 25, 26): (' må det nu gå dina fiender och dem som söka bereda min herre ofärd såsom det må gå Nabal.', ' må det nu gå dina fiender och dem som söka bereda min herre ofärd, såsom det må gå Nabal.'),  # SAM1-229
    ("1Sam", 25, 28): ('Förlåt din tjänarinna vad hon har brutit. Ty HERREN skall förvisso åt min herre uppbygga ett hus som bliver beståndande,', 'Förlåt din tjänarinna, vad hon har brutit. Ty HERREN skall förvisso åt min herre uppbygga ett hus, som bliver beståndande,'),  # SAM1-230
    ("1Sam", 25, 30): ('När nu HERREN gör med min herre allt det goda varom han har talat till dig,', 'När nu HERREN gör med min herre allt det goda, varom han har talat till dig,'),  # SAM1-231
    ("1Sam", 25, 32): ('Då sade David till Abigal: »Välsignad vare HERREN, Israels Gud som i dag har sänt dig mig till mötes!', 'Då sade David till Abigail: »Välsignad vare HERREN, Israels Gud, som i dag har sänt dig mig till mötes!'),  # SAM1-232
    ("1Sam", 25, 39): ('När David hörde att Nabal var död, sade han: »Lovad vare HERREN, som på Nabal har hämnats den smälek han tillfogade mig, och som har bevarat sin tjänare från att göra vad ont var, under det att HERREN lät Nabals ondska komma tillbaka över hans eget huvud!» Och David sände åstad och lät säga Abigail att han önskade få henne till sin hustru.', 'När David hörde, att Nabal var död, sade han: »Lovad vare HERREN, som på Nabal har hämnats den smälek han tillfogade mig, och som har bevarat sin tjänare från att göra vad ont var, under det att HERREN lät Nabals ondska komma tillbaka över hans eget huvud!» Och David sände åstad och lät säga Abigail, att han önskade få henne till sin hustru.'),  # SAM1-233
    ("1Sam", 25, 42): (' likaledes de fem tärnor som utgjorde hennes följe.', ' likaledes de fem tärnor, som utgjorde hennes följe.'),  # SAM1-234
    ("1Sam", 26, 2): ('Då bröt Saul upp och drog ned till öknen Sif med tre tusen män utvalda ur Israel,', 'Då bröt Saul upp och drog ned till öknen Sif med tre tusen män, utvalda ur Israel,'),  # SAM1-235
    ("1Sam", 26, 3): (' Och när David förnam att Saul hade kommit efter honom in i öknen,', ' Och när David förnam, att Saul hade kommit efter honom in i öknen,'),  # SAM1-236
    ("1Sam", 26, 5): ('Då bröt David upp och begav sig till det ställe där Saul hade lägrat sig; och David såg platsen där Saul låg med sin härhövitsman Abner, Ners son. Saul låg nämligen i vagnborgen, och folket var lägrat runt', 'Då bröt David upp och begav sig till det ställe, där Saul hade lägrat sig; och David såg platsen, där Saul låg med sin härhövitsman Abner, Ners son. Saul låg nämligen i vagnborgen, och folket var lägrat runt omkring honom.'),  # SAM1-237
    ("1Sam", 26, 7): ('Så kommo då David och Abisai om natten till folket där,', 'Så kommo då David och Abisai om natten till folket där'),  # SAM1-238
    ("1Sam", 26, 11): ('men HERREN låte det vara fjärran ifrån mig att jag skulle uträcka min hand mot HERRENS smorde. Tag nu likväl spjutet som står vid hans huvudgärd och vattenkruset; och låt oss sedan gå vår väg.', 'men HERREN låte det vara fjärran ifrån mig, att jag skulle uträcka min hand mot HERRENS smorde. Tag nu likväl spjutet, som står vid hans huvudgärd, och vattenkruset; och låt oss sedan gå vår väg.»'),  # SAM1-239
    ("1Sam", 26, 13): (' långt ifrån så att avståndet var stort mellan dem.', ' långt ifrån, så att avståndet var stort mellan dem.'),  # SAM1-240
    ("1Sam", 26, 15): ('»Du är ju en man som icke har sin like i Israel.', '»Du är ju en man, som icke har sin like i Israel.'),  # SAM1-241
    ("1Sam", 26, 16): (' var äro konungens spjut och vattenkruset som stodo vid hans huvudgärd?', ' var äro konungens spjut och vattenkruset, som stodo vid hans huvudgärd?'),  # SAM1-242
    ("1Sam", 26, 19): (' Om det är HERREN som har uppeggat dig emot mig,', ' Om det är HERREN, som har uppeggat dig emot mig,'),  # SAM1-243
    ("1Sam", 27, 4): ('Och när det blev berättat för Saul att David hade flytt till Gat,', 'Och när det blev berättat för Saul, att David hade flytt till Gat,'),  # SAM1-244
    ("1Sam", 27, 12): ('Därför trodde Akis David och tänkte: »Han har nu gjort sig förhatlig för sitt folk Israel och kommer att bliva min tjänare för alltid.', 'Därför trodde Akis David och tänkte: »Han har nu gjort sig förhatlig för sitt folk Israel och kommer att bliva min tjänare för alltid.»'),  # SAM1-245
    ("1Sam", 28, 1): ('»Du må veta att du med dina män nu måste draga ut med mig i härnad.', '»Du må veta, att du med dina män nu måste draga ut med mig i härnad.'),  # SAM1-246
    ("1Sam", 28, 2): (' Akis sade till David ', ' Akis sade till David: '),  # SAM1-247
    ("1Sam", 28, 14): ('»Det är en gammal man som kommer upp, höljd i en kåpa.» Då förstod Saul att det var Samuel,', '»Det är en gammal man, som kommer upp, höljd i en kåpa.» Då förstod Saul, att det var Samuel,'),  # SAM1-248
    ("1Sam", 28, 17): ('HERREN har efter sitt behag gjort vad han hade sagt genom mig:', 'HERREN har efter sitt behag gjort, vad han hade sagt genom mig:'),  # SAM1-249
    ("1Sam", 28, 21): (' och när hon såg huru högeligen förskräckt han var, sade hon till honom: »Se, din tjänarinna lyssnade till din begäran; Jag tog min själ i min hand och hörsammade den önskan du uttalade till mig.', ' och när hon såg, huru högeligen förskräckt han var, sade hon till honom: »Se, din tjänarinna lyssnade till din begäran; jag tog min själ i min hand och hörsammade den önskan du uttalade till mig.'),  # SAM1-250
    ("1Sam", 28, 22): ('Så lyssna nu också du till dina tjänarinnas ord och låt mig sätta fram litet mat för dig,', 'Så lyssna nu också du till din tjänarinnas ord och låt mig sätta fram litet mat för dig,'),  # SAM1-251
    ("1Sam", 28, 25): ('Sedan satte hon fram det för Saul:', 'Sedan satte hon fram det för Saul'),  # SAM1-252
    ("1Sam", 29, 3): ('sade filistéernas furstar: »Vad hava dessa hebréer här att göra?» Men Akis svarade filistéernas furstar: »Denne David är ju Sauls, Israels konungs, tjänare, som nu har varit hos mig över år och dag, och jag har icke funnit något ont hos honom, från den dag han gick över till mig ända till denna dag.', 'sade filistéernas furstar: »Vad hava dessa hebréer här att göra?» Men Akis svarade filistéernas furstar: »Denne David är ju Sauls, Israels konungs, tjänare, som nu har varit hos mig över år och dag, och jag har icke funnit något ont hos honom, från den dag han gick över till mig ända till denna dag.»'),  # SAM1-253
    ("1Sam", 29, 5): ('Han är ju den David till vilkens ära man sjunger så under dansen:', 'Han är ju den David, till vilkens ära man sjunger så under dansen:'),  # SAM1-254
    ("1Sam", 29, 9): ('»Jag vet bäst att du är mig välbehaglig såsom en Guds ängel;', '»Jag vet bäst, att du är mig välbehaglig såsom en Guds ängel;'),  # SAM1-255
    ("1Sam", 29, 10): ('Så stå nu upp bittida i morgon,', 'Så stå nu upp bittida i morgon'),  # SAM1-256
    ("1Sam", 30, 2): ('Och kvinnorna som voro därinne,', 'Och kvinnorna, som voro därinne,'),  # SAM1-257
    ("1Sam", 30, 3): ('När nu David med sina män kom till staden och fick se att den var uppbränd i eld,', 'När nu David med sina män kom till staden och fick se, att den var uppbränd i eld,'),  # SAM1-258
    ("1Sam", 30, 10): (' ty de som sade blivit för trötta,', ' ty de som hade blivit för trötta,'),  # SAM1-259
    ("1Sam", 30, 15): ('»Lova mig med ed vid Gud att du icke dödar mig eller utlämnar mig åt min herre,', '»Lova mig med ed vid Gud, att du icke dödar mig eller utlämnar mig åt min herre,'),  # SAM1-260
    ("1Sam", 30, 21): (' gingo dessa åstad för att möta David och det folk som han hade med sig;', ' gingo dessa åstad för att möta David och det folk, som han hade med sig;'),  # SAM1-261
    ("1Sam", 30, 25): ('Och därvid blev det,', 'Och därvid blev det'),  # SAM1-262
    ("1Sam", 30, 31): ('till de äldste i Hebron och till alla de orter där David hade vandrat omkring med sina män.', 'till de äldste i Hebron och till alla de orter, där David hade vandrat omkring med sina män.'),  # SAM1-263
    ("1Sam", 31, 5): ('Men när vapendragaren såg att Saul var död,', 'Men när vapendragaren såg, att Saul var död,'),  # SAM1-264
    ("1Sam", 31, 7): ('Och när israeliterna på andra sidan dalen och på andra sidan Jordan förnummo att Israels män hade flytt,', 'Och när israeliterna på andra sidan dalen och på andra sidan Jordan förnummo, att Israels män hade flytt,'),  # SAM1-265
    ("1Sam", 31, 11): ('Men när invånarna i Jabes i Gilead hörde vad filistéerna hade gjort med Saul,', 'Men när invånarna i Jabes i Gilead hörde, vad filistéerna hade gjort med Saul,'),  # SAM1-266
    ("1Sam", 31, 12): (' och gingo hela natten och togo Sauls och hans söners kroppar ned från Bet-Sans mur,', ' och gingo hela natten och togo Sauls och hans söners kroppar ned från Bet-Sans mur'),  # SAM1-267
}
