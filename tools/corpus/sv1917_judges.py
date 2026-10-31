"""Judges corrections JDG001–JDG168, checked against EFS 1927 and 1926 images.
Evidence and separate human-review boxes: docs/reports/sv1917-errata-judges.md.
Applied by runeberg1917.read with its existing exactly-once guard.
"""

REPLACE = {
    ('Judg', 1, 17): ('slogo de kananéer som bodde i', 'slogo de kananéer, som bodde i'),  # JDG001
    ('Judg', 1, 21): ('de göra ännu dag.', 'de göra ännu i dag.'),  # JDG002
    ('Judg', 1, 24): ('honom: »Visa oss var vi kunna', 'honom: »Visa oss, var vi kunna'),  # JDG003
    ('Judg', 1, 25): ('hade visat dem var de kunde komma in i staden, slog de stadens invånare', 'hade visat dem, var de kunde komma in i staden, slogo de stadens invånare'),  # JDG004
    ('Judg', 1, 29): ('Efraim de kananéer som bodde i', 'Efraim de kananéer, som bodde i'),  # JDG005
    ('Judg', 2, 9): ('område i Timna-Heres i Efraims', 'område i Timnat-Heres i Efraims'),  # JDG006
    ('Judg', 2, 11): ('Israels barn vad ont var i HERRENS ögon och tjänade', 'Israels barn, vad ont var i HERRENS ögon, och tjänade'),  # JDG007
    ('Judg', 2, 14): ('HERRENS vrede mot i Israel, och han', 'HERRENS vrede mot Israel, och han'),  # JDG008
    ('Judg', 2, 15): ('HERREN hade svurit att det skulle', 'HERREN hade svurit, att det skulle'),  # JDG009
    ('Judg', 2, 17): ('hade vandrat, i lydnad för', 'hade vandrat i lydnad för'),  # JDG010
    ('Judg', 2, 19): ('togo sig till vad fördärvligt', 'togo sig till, vad fördärvligt'),  # JDG011
    ('Judg', 2, 20): ('överträtt det förbund som jag stadgade', 'överträtt det förbund, som jag stadgade'),  # JDG012
    ('Judg', 3, 1): ('voro de folk som HERREN lät bliva kvar, för att genom dem sätta Israel på prov, alla de israeliter nämligen, som icke hade varit med om alla krigen i Kanaan', 'voro de folk, som HERREN lät bliva kvar, för att genom dem sätta Israel på prov, alla de israeliter nämligen, som icke hade varit med om alla krigen i Kanaan.'),  # JDG013
    ('Judg', 3, 3): ('och sidonier, samt de hivéer som bodde i', 'och sidonier samt de hivéer, som bodde i'),  # JDG014
    ('Judg', 3, 4): ('att förnimma om de ville hörsamma de bud som han hade givit deras fäder.', 'att förnimma, om de ville hörsamma de bud, som han hade givit deras fäder genom Mose.'),  # JDG015
    ('Judg', 3, 5): ('Israels barn bodde: ibland kananéerna,', 'Israels barn bodde ibland kananéerna,'),  # JDG016
    ('Judg', 3, 7): ('Israels barn vad ont var i HERRENS ögon och glömde HERREN,', 'Israels barn, vad ont var i HERRENS ögon, och glömde HERREN,'),  # JDG017
    ('Judg', 3, 12): ('barn gjorde åter vad ont var i HERRENS ögon, då gav HERREN Eglon, konungen i Moab, makt över Israel, eftersom de gjorde vad ont var', 'barn gjorde åter, vad ont var i HERRENS ögon; då gav HERREN Eglon, konungen i Moab, makt över Israel, eftersom de gjorde, vad ont var'),  # JDG018
    ('Judg', 3, 14): ('måste nu tjäna: Eglon, konungen', 'måste nu tjäna Eglon, konungen'),  # JDG019
    ('Judg', 3, 18): ('lät han folket som hade burit dem gå sin väg.', 'lät han folket, som hade burit dem, gå sin väg.'),  # JDG020
    ('Judg', 3, 24): ('när de fingo se att dörrarna', 'när de fingo se, att dörrarna'),  # JDG021
    ('Judg', 3, 27): ('stöta i basun Efraims bergsbygd;', 'stöta i basun i Efraims bergsbygd;'),  # JDG022
    ('Judg', 4, 5): ('till henne, for att hon skulle', 'till henne, för att hon skulle'),  # JDG023
    ('Judg', 4, 12): ('berättade för Sisera att Barak, Abinoams', 'berättade för Sisera, att Barak, Abinoams'),  # JDG024
    ('Judg', 4, 14): ('detta är den dag på vilken HERREN', 'detta är den dag, på vilken HERREN'),  # JDG025
    ('Judg', 4, 19): ('öppnade hon mjölk- kärlet och gav', 'öppnade hon mjölkkärlet och gav'),  # JDG026
    ('Judg', 4, 20): ('och frågar dig om någon är', 'och frågar dig, om någon är'),  # JDG027
    ('Judg', 4, 22): ('visa dig den man som du söker.»', 'visa dig den man, som du söker.»'),  # JDG028
    ('Judg', 5, 1): ('De sjöngo Debora', 'Då sjöngo Debora'),  # JDG029
    ('Judg', 5, 14): ('Efraim kommo män som hade rotfäst sig i Amalek; Benjamin följde dig och blandade sig med dina skaror. Ned ifrån Makir drogo hövdingar åstad, och från Sebulon män som buro anförarstav.', 'Efraim kommo män, som hade rotfäst sig i Amalek; Benjamin följde dig och blandade sig med dina skaror. Ned ifrån Makir drogo hövdingar åstad, och från Sebulon män, som buro anförarstav.'),  # JDG030
    ('Judg', 5, 17): ('Jordan. Och Dan varför – dröjer han ännu', 'Jordan. Och Dan – varför dröjer han ännu'),  # JDG031
    ('Judg', 5, 18): ('var ett folk som prisgav', 'var ett folk, som prisgav'),  # JDG032
    ('Judg', 5, 19): ('fram och stridde ja, då stridde', 'fram och stridde, ja, då stridde'),  # JDG033
    ('Judg', 5, 26): ('arbetshammaren med den slog', 'arbetshammaren; med den slog'),  # JDG034
    ('Judg', 6, 1): ('Israels barn gjorde vad ont var', 'Israels barn gjorde, vad ont var'),  # JDG035
    ('Judg', 6, 10): ('frukta de gudar som dyrkas av amoréerna, i vilkas land I bon. Men I villen icke höra min röst.', 'frukta de gudar, som dyrkas av amoréerna, i vilkas land I bon. Men I villen icke höra min röst.»'),  # JDG036
    ('Judg', 6, 15): ('oansenligaste i Manasse, och jag själv', 'oansenligaste i Manasse och jag själv'),  # JDG037
    ('Judg', 6, 16): ('Midjan, såsom voro det en enda', 'Midjan, såsom vore det en enda'),  # JDG038
    ('Judg', 6, 17): ('få ett tecken att det är du', 'få ett tecken, att det är du'),  # JDG039
    ('Judg', 6, 20): ('till honom: Tag köttet och', 'till honom: »Tag köttet och'),  # JDG040
    ('Judg', 6, 21): ('räckte ut staven som han hade i sin hand och rörde med', 'räckte ut staven, som han hade i sin hand, och rörde med'),  # JDG041
    ('Judg', 6, 22): ('Då såg Gideon att det var', 'Då såg Gideon, att det var'),  # JDG042
    ('Judg', 6, 25): ('»Tag den tjur som tillhör din fader och den andra sjuåriga tjuren. Riv sedan ned det Baalsaltare som tillhör din fader, och hugg sönder Aseran som står därinvid.', '»Tag den tjur, som tillhör din fader, och den andra sjuåriga tjuren. Riv sedan ned det Baalsaltare som tillhör din fader, och hugg sönder Aseran, som står därinvid.'),  # JDG043
    ('Judg', 6, 27): ('såsom HERREN sade sagt till', 'såsom HERREN hade sagt till'),  # JDG044
    ('Judg', 6, 28): ('morgon fingo mannen i staden se att Baals altare', 'morgon fingo männen i staden se, att Baals altare'),  # JDG045
    ('Judg', 6, 29): ('fingo de veta att Gideon,', 'fingo de veta, att Gideon,'),  # JDG046
    ('Judg', 6, 30): ('sönder Aseran som stod därinvid.»', 'sönder Aseran, som stod därinvid.»'),  # JDG047
    ('Judg', 6, 31): ('hans altare.', 'hans altare.»'),  # JDG048
    ('Judg', 6, 37): ('torr, då vet jag att du genom', 'torr, då vet jag, att du genom'),  # JDG049
    ('Judg', 7, 2): ('Gideon: »Folket som har följt dig är för talrikt', 'Gideon: »Folket, som har följt dig, är för talrikt'),  # JDG050
    ('Judg', 7, 4): ('men var och en om vilken jag', 'men var och en, om vilken jag'),  # JDG051
    ('Judg', 7, 13): ('dröm. Jag tyckte att en kornbrödskaka', 'dröm. Jag tyckte, att en kornbrödskaka'),  # JDG052
    ('Judg', 7, 15): ('i eder hand.', 'i eder hand.»'),  # JDG053
    ('Judg', 7, 19): ('krossade krukorna som de hade', 'krossade krukorna, som de hade'),  # JDG054
    ('Judg', 7, 25): ('Seebspressen, och förföljde', 'Seebspressen och förföljde'),  # JDG055
    ('Judg', 8, 3): ('hand var det som Gud gav', 'hand var det, som Gud gav'),  # JDG056
    ('Judg', 8, 6): ('eftersom du fordrar att vi skola', 'eftersom du fordrar, att vi skola'),  # JDG057
    ('Judg', 8, 7): ('Gideon sade »Nåväl; när HERREN, giver Seba och', 'Gideon sade: »Nåväl; när HERREN giver Seba och'),  # JDG058
    ('Judg', 8, 15): ("sade han: »Se här äro nu Seba och Salmunna, om vilka I hånfullt saden till mig: 'Har du redan Seba och Salmunna i ditt våld, eftersom du fordrar att vi skola", "sade han: »Se, här äro nu Seba och Salmunna, om vilka I hånfullt saden till mig: 'Har du redan Seba och Salmunna i ditt våld, eftersom du fordrar, att vi skola"),  # JDG059
    ('Judg', 8, 18): ('»Hurudana voro de män som I dräpten', '»Hurudana voro de män, som I dräpten'),  # JDG060
    ('Judg', 8, 21): ('de prydnader som sutto på', 'de prydnader, som sutto på'),  # JDG061
    ('Judg', 8, 31): ('En bihustru som han hade i Sikem födde honom', 'En bihustru, som han hade i Sikem, födde honom'),  # JDG062
    ('Judg', 8, 32): ('abiesritiska Ofra', 'abiesritiska Ofra.'),  # JDG063
    ('Judg', 9, 2): ('därjämte ihåg att jag är edert', 'därjämte ihåg, att jag är edert'),  # JDG064
    ('Judg', 9, 7): ('berättade detta far Jotam, gick', 'berättade detta för Jotam, gick'),  # JDG065
    ('Judg', 9, 8): ("konung över oss'", "konung över oss.'"),  # JDG066
    ('Judg', 9, 17): ('ty I veten att min fader', 'ty I veten, att min fader'),  # JDG067
    ('Judg', 9, 27): ('hände sig en gång att de gingo ut på fältet och avbärgade sina vingårdar och pressade druvorna och höllo en glädjefest, och de gingo därvid in i sin guds hus och åto och drucko, och uttalade', 'hände sig en gång, att de gingo ut på fältet och avbärgade sina vingårdar och pressade druvorna och höllo en glädjefest, och de gingo därvid in i sin guds hus och åto och drucko och uttalade'),  # JDG068
    ('Judg', 9, 28): ('Nej, tjänen män som härstamma', 'Nej, tjänen män, som härstamma'),  # JDG069
    ('Judg', 9, 30): ('staden, fick höra vad Gaal, Ebeds', 'staden, fick höra, vad Gaal, Ebeds'),  # JDG070
    ('Judg', 9, 33): ('göra med honom vad tillfället', 'göra med honom, vad tillfället'),  # JDG071
    ('Judg', 9, 38): ('kommer det folk som du så föraktade. Drag nu ut och strid mot dem.', 'kommer det folk, som du så föraktade. Drag nu ut och strid mot dem.»'),  # JDG072
    ('Judg', 9, 43): ('när han fick se att folket gick', 'när han fick se, att folket gick'),  # JDG073
    ('Judg', 9, 45): ('dräpte det folk som fanns därinne Sedan rev han', 'dräpte det folk, som fanns därinne. Sedan rev han'),  # JDG074
    ('Judg', 9, 47): ('blev berättat for Abimelek att hela besättningen', 'blev berättat för Abimelek, att hela besättningen'),  # JDG075
    ('Judg', 9, 49): ('de människor som bodde i', 'de människor, som bodde i'),  # JDG076
    ('Judg', 9, 55): ('israeliterna sågo att Abimelek', 'israeliterna sågo, att Abimelek'),  # JDG077
    ('Judg', 9, 56): ('Gud det onda som Abimelek', 'Gud det onda, som Abimelek'),  # JDG078
    ('Judg', 9, 57): ('allt det onda som Sikems män hade gjort lät Gud ock', 'allt det onda, som Sikems män hade gjort, lät Gud ock'),  # JDG079
    ('Judg', 10, 6): ('barn gjorde åter vad ont var i HERRENS ögon och tjänade', 'barn gjorde åter, vad ont var i HERRENS ögon, och tjänade'),  # JDG080
    ('Judg', 10, 14): ('nu ären i nöd.', 'nu ären i nöd.»'),  # JDG081
    ('Judg', 10, 15): ('gör du med oss alldeles såsom', 'gör du med oss, alldeles såsom'),  # JDG082
    ('Judg', 11, 10): ('låta det bliva så om du har sagt.»', 'låta det bliva, såsom du har sagt.»'),  # JDG083
    ('Judg', 11, 17): ('ville icke heller Då stannade', 'ville icke heller. Då stannade'),  # JDG084
    ('Judg', 11, 20): ('Jahas; där in- lät han sig i', 'Jahas; där inlät han sig i'),  # JDG085
    ('Judg', 11, 22): ('ända till Jabbok, och från öknen', 'ända till Jabbok och från öknen'),  # JDG086
    ('Judg', 11, 25): ('Menar du att du är så mycket förmer än Balak, Sippors sons konungen i Moab?', 'Menar du, att du är så mycket förmer än Balak, Sippors son, konungen i Moab?'),  # JDG087
    ('Judg', 11, 31): ('så lovar jag att vadhelst', 'så lovar jag, att vadhelst'),  # JDG088
    ('Judg', 11, 34): ('hans enda barn, han hade utom', 'hans enda barn; han hade utom'),  # JDG089
    ('Judg', 11, 37): ('mina väninnor.', 'mina väninnor.»'),  # JDG090
    ('Judg', 11, 40): ('sedvänja i Israel att Israels', 'sedvänja i Israel, att Israels'),  # JDG091
    ('Judg', 12, 1): ('själv i eld.', 'själv i eld.»'),  # JDG092
    ('Judg', 12, 3): ('Och när jag såg att I icke villen', 'Och när jag såg, att I icke villen'),  # JDG093
    ('Judg', 13, 1): ('filistéernas hand, i fyrtio år.', 'filistéernas hand i fyrtio år.'),  # JDG094
    ('Judg', 13, 4): ('starka drycker ej heller äter', 'starka drycker, ej heller äter'),  # JDG095
    ('Judg', 13, 5): ('han skall göra begynnelse till', 'han skall göra en begynnelse till'),  # JDG096
    ('Judg', 13, 6): ('frågade honom icke varifrån han', 'frågade honom icke, varifrån han'),  # JDG097
    ('Judg', 13, 8): ('låt gudsmannen som du sände hit åter komma till oss, för att han må lära oss huru vi skola', 'låt gudsmannen, som du sände hit, åter komma till oss, för att han må lära oss, huru vi skola'),  # JDG098
    ('Judg', 13, 10): ('honom: »Mannen som kom till mig häromdagen har uppenbarat', 'honom: »Mannen, som kom till mig häromdagen, har uppenbarat'),  # JDG099
    ('Judg', 13, 11): ('»Är du den man som förut talade', '»Är du den man, som förut talade'),  # JDG100
    ('Judg', 13, 12): ('med gossen? Hur skall man göra', 'med gossen? Huru skall man göra'),  # JDG101
    ('Judg', 13, 13): ('svarade Manoa »Din hustru skall taga sig till vara för allt varom jag har', 'svarade Manoa: »Din hustru skall taga sig till vara för allt, varom jag har'),  # JDG102
    ('Judg', 13, 14): ('något som ha vuxit på vinträd,', 'något som har vuxit på vinträd,'),  # JDG103
    ('Judg', 13, 16): ('förstod icke att det var', 'förstod icke, att det var'),  # JDG104
    ('Judg', 13, 20): ('sitt ansikte', 'sitt ansikte.'),  # JDG105
    ('Judg', 13, 21): ('förstod Manoa att det hade', 'förstod Manoa, att det hade'),  # JDG106
    ('Judg', 13, 23): ('spisoffer av vår hand, och icke låtit', 'spisoffer av vår hand och icke låtit'),  # JDG107
    ('Judg', 14, 4): ('moder visste icke att detta kom', 'moder visste icke, att detta kom'),  # JDG108
    ('Judg', 14, 6): ('fader och moder vad han hade', 'fader och moder, vad han hade'),  # JDG109
    ('Judg', 14, 9): ('medan han gick, han kom så till sin fader och moder och gav dem, och de åto. Men han talade icke om för dem att det var', 'medan han gick; han kom så till sin fader och moder och gav dem, och de åto. Men han talade icke om för dem, att det var'),  # JDG110
    ('Judg', 15, 1): ('besöka sin hustru, och förde med', 'besöka sin hustru och förde med'),  # JDG111
    ('Judg', 15, 2): ('höll för säkert att du hade', 'höll för säkert, att du hade'),  # JDG112
    ('Judg', 15, 10): ('göra mot honom såsom han har', 'göra mot honom, såsom han har'),  # JDG113
    ('Judg', 15, 11): ('Simson: »Du vet ju att filistéerna', 'Simson: »Du vet ju, att filistéerna'),  # JDG114
    ('Judg', 15, 15): ('ihjäl tusen män.', 'ihjäl tusen man.'),  # JDG115
    ('Judg', 15, 18): ('jag dö av törst, och så falla', 'jag dö av törst och så falla'),  # JDG116
    ('Judg', 16, 2): ('gasiterna fingo höra att Simson hade', 'gasiterna fingo höra, att Simson hade'),  # JDG117
    ('Judg', 16, 3): ('jämte bommen, och lade alltsammans', 'jämte bommen och lade alltsammans'),  # JDG118
    ('Judg', 16, 5): ('uppenbara för dig varav det beror att han är så', 'uppenbara för dig, varav det beror, att han är så'),  # JDG119
    ('Judg', 16, 6): ('Simson: »Säg mig varav det beror att du är så', 'Simson: »Säg mig, varav det beror, att du är så'),  # JDG120
    ('Judg', 16, 13): ('mig; säg mig nu huru man skulle', 'mig; säg mig nu, huru man skulle'),  # JDG121
    ('Judg', 16, 15): ('»Huru kan du säga att du har mig kär, du som icke är uppriktig mot mig? Tre gånger har du nu bedragit mig och icke velat säga mig varpå det beror att du är så', '»Huru kan du säga, att du har mig kär, du som icke är uppriktig mot mig? Tre gånger har du nu bedragit mig och icke velat säga mig, varpå det beror, att du är så'),  # JDG122
    ('Judg', 16, 16): ('han så otålig att han kunde', 'han så otålig, att han kunde'),  # JDG123
    ('Judg', 16, 18): ('Delila insåg att han hade', 'Delila insåg, att han hade'),  # JDG124
    ('Judg', 16, 19): ('till sig en man som på hennes', 'till sig en man, som på hennes'),  # JDG125
    ('Judg', 16, 20): ('han visste icke att HERREN hade', 'han visste icke, att HERREN hade'),  # JDG126
    ('Judg', 16, 24): ('fiende i vår hand honom som förödde', 'fiende i vår hand, honom som förödde'),  # JDG127
    ('Judg', 17, 1): ('levde en man som hette Mika.', 'levde en man, som hette Mika.'),  # JDG128
    ('Judg', 17, 2): ('silversiklar som blevo dig fråntagna, och för vilkas skull du uttalade en förbannelse, som jag själv hörde, se, de penningarna finnas hos mig. Det var jag som tog dem.»', 'silversiklar, som blevo dig fråntagna, och för vilkas skull du uttalade en förbannelse, som jag själv hörde, se, de penningarna finnas hos mig. Det var jag, som tog dem.»'),  # JDG129
    ('Judg', 17, 8): ('Juda, för att se om han funne någon annan ort där han kunde', 'Juda, för att se, om han funne någon annan ort, där han kunde'),  # JDG130
    ('Judg', 17, 9): ('vandring, för att se om jag finner någon annan ort där jag kan', 'vandring, för att se, om jag finner någon annan ort, där jag kan'),  # JDG131
    ('Judg', 17, 10): ('kläder du behöver, och därtill', 'kläder du behöver och därtill'),  # JDG132
    ('Judg', 17, 13): ('sade: »Nu vet jag att HERREN skall', 'sade: »Nu vet jag, att HERREN skall'),  # JDG133
    ('Judg', 18, 5): ('att vi få veta om den resa som vi äro stadda på skall bliva', 'att vi få veta, om den resa, som vi äro stadda på, skall bliva'),  # JDG134
    ('Judg', 18, 6): ('frid. Den resa som I ären stadda på står under HERRENS', 'frid. Den resa, som I ären stadda på, står under HERRENS'),  # JDG135
    ('Judg', 18, 14): ("De fem män som hade varit åstad för att bespeja Lais' land togo då till orda och sade till sina bröder: »I mån veta att här i husen finnas en efod och husgudar och en skuren och en gjuten Gudabild. Så betänken nu vad I bören", "De fem män, som hade varit åstad för att bespeja Lais' land, togo då till orda och sade till sina bröder: »I mån veta, att här i husen finnas en efod och husgudar och en skuren och en gjuten gudabild. Så betänken nu, vad I bören"),  # JDG136
    ('Judg', 18, 17): ('Och de fem män som hade varit åstad för att bespeja landet gingo upp och', 'Och de fem män, som hade varit åstad för att bespeja landet, gingo upp och'),  # JDG137
    ('Judg', 18, 21): ('gingo vidare, och läto därvid', 'gingo vidare och läto därvid'),  # JDG138
    ('Judg', 18, 24): ('tagit de gudar som jag har', 'tagit de gudar, som jag har'),  # JDG139
    ('Judg', 18, 25): ('kan det hända att några män', 'kan det hända, att några män'),  # JDG140
    ('Judg', 18, 26): ('när Mika såg att de voro', 'när Mika såg, att de voro'),  # JDG141
    ('Judg', 18, 31): ('skurna gudabild som Mika hade', 'skurna gudabild, som Mika hade'),  # JDG142
    ('Judg', 19, 9): ('här över natten, och låt ditt', 'här över natten och låt ditt'),  # JDG143
    ('Judg', 19, 10): ('reste sin väg, och kom så fram', 'reste sin väg och kom så fram'),  # JDG144
    ('Judg', 19, 22): ('hitut den man som har kommit', 'hitut den man, som har kommit'),  # JDG145
    ('Judg', 19, 23): ('Då gick mannen som rådde om huset ut till dem', 'Då gick mannen, som rådde om huset, ut till dem'),  # JDG146
    ('Judg', 19, 24): ('har en dotter som är jungfru, och han har själv en bihustru. Dem vill jag föra hitut, så kunnen I kränka dem och göra med dem vad I finnen för gott. Men med denne man mån I icke göra någon sådan galenskap.', 'har en dotter, som är jungfru, och han har själv en bihustru. Dem vill jag föra hitut, så kunnen I kränka dem och göra med dem, vad I finnen för gott. Men med denne man mån I icke göra någon sådan galenskap.»'),  # JDG147
    ('Judg', 19, 29): ('och tog sin bi- hustru och styckade', 'och tog sin bihustru och styckade'),  # JDG148
    ('Judg', 19, 30): ('ifrån den dag då Israels barn', 'ifrån den dag, då Israels barn'),  # JDG149
    ('Judg', 20, 1): ('Israels barn ut, och menigheten', 'Israels barn ut; och menigheten'),  # JDG150
    ('Judg', 20, 2): ('i hela folket alla Israels stammar, trädde fram i Guds folks församling: fyra hundra tusen svärdbeväpnade mån till fots.', 'i hela folket, alla Israels stammar, trädde fram i Guds folks församling: fyra hundra tusen svärdbeväpnade män till fots.'),  # JDG151
    ('Judg', 20, 3): ('barn fingo höra att de övriga israeliterna hade dragit upp till Mispa. Och Israels barn sade: »Omtalen huru denna ogärning', 'barn fingo höra, att de övriga israeliterna hade dragit upp till Mispa. Och Israels barn sade: »Omtalen, huru denna ogärning'),  # JDG152
    ('Judg', 20, 9): ('Detta är vad vi nu vilja', 'Detta är, vad vi nu vilja'),  # JDG153
    ('Judg', 20, 10): ('göra med staden såsom tillbörligt', 'göra med staden, såsom tillbörligt'),  # JDG154
    ('Judg', 20, 12): ('och läto säga; »Vad är det för en ogärning som har blivit', 'och läto säga: »Vad är det för en ogärning, som har blivit'),  # JDG155
    ('Judg', 20, 15): ('Benjamins barn, de utgjorde', 'Benjamins barn; de utgjorde'),  # JDG156
    ('Judg', 20, 16): ('hundra utvalda män som voro vänsterhänta; alla dessa kunde med slungstenen träffa på håret, utan att fela.', 'hundra utvalda män, som voro vänsterhänta; alla dessa kunde med slungstenen träffa på håret utan att fela.'),  # JDG157
    ('Judg', 20, 32): ('ut på vägarna.', 'ut på vägarna.»'),  # JDG158
    ('Judg', 20, 33): ('från platsen där de voro, och hade ställt upp sig i slagordning vid Baal-Tamar, under det att de israeliter som lågo i bakhåll bröto fram ifrån', 'från platsen, där de voro, och hade ställt upp sig i slagordning vid Baal-Tamar, under det att de israeliter som lågo i bakhåll, bröto fram ifrån'),  # JDG159
    ('Judg', 20, 34): ('då tio tusen man, utvalda ur hela Israel, fram gent emot Gibea, och striden blev hård, utan att någon visste att olyckan', 'då tio tusen män, utvalda ur hela Israel, fram gent emot Gibea, och striden blev hård, utan att någon visste, att olyckan'),  # JDG160
    ('Judg', 20, 36): ('Benjamins barn att de voro', 'Benjamins barn, att de voro'),  # JDG161
    ('Judg', 20, 39): ('av Israels man, kanhända ett', 'av Israels män, kanhända ett'),  # JDG162
    ('Judg', 20, 41): ('ty nu sågo de att olyckan', 'ty nu sågo de, att olyckan'),  # JDG163
    ('Judg', 21, 5): ('och Israels barn', 'Och Israels barn'),  # JDG164
    ('Judg', 21, 8): ('Och då frågade åter:', 'Och de frågade åter:'),  # JDG165
    ('Judg', 21, 9): ('mönstrades, befanns det att ingen av', 'mönstrades, befanns det, att ingen av'),  # JDG166
    ('Judg', 21, 11): ('mankön att skaffa skolen I giva till spillo.', 'mankön att skaffa, skolen I giva till spillo.»'),  # JDG167
    ('Judg', 21, 19): ('vidare: »En HERREN högtid plägar', 'vidare: »En HERRENS högtid plägar'),  # JDG168
}
