"""1 Kings corrections, checked against EFS 1927 and 1926 print images.
Evidence and separate human review: docs/reports/sv1917-errata-1kings.md.
Applied by runeberg1917.read with its existing exactly-once guard.
"""

REPLACE = {
    ('1Kgs', 1, 2): ('konungen varm»', 'konungen varm.»'),  # KGS1-001
    ('1Kgs', 1, 5): ('ock femtio man som löpte framför', 'ock femtio man, som löpte framför'),  # KGS1-002
    ('1Kgs', 1, 7): ('och med prästens Ebjatar, och', 'och med prästen Ebjatar, och'),  # KGS1-003
    ('1Kgs', 1, 9): ('alla de Juda män som voro i konungens', 'alla de Juda män, som voro i konungens'),  # KGS1-004
    ('1Kgs', 1, 11): ('har väl hört att Adonia,', 'har väl hört, att Adonia,'),  # KGS1-005
    ('1Kgs', 1, 20): ('kungöra för dem vem som skall', 'kungöra för dem, vem som skall'),  # KGS1-006
    ('1Kgs', 1, 24): ('som har sagt att Adonia skall', 'som har sagt, att Adonia skall'),  # KGS1-007
    ('1Kgs', 1, 27): ('dina tjänare vet vem som skall', 'dina tjänare veta, vem som skall'),  # KGS1-008
    ('1Kgs', 1, 32): ('Jojadas son. När dessa kommo', 'Jojadas son.» När dessa kommo'),  # KGS1-009
    ('1Kgs', 1, 33): ('fören honom med till Gihon.', 'fören honom ned till Gihon.'),  # KGS1-010
    ('1Kgs', 1, 41): ('alla de inbjudna som han hade hos sig hörde detta,', 'alla de inbjudna, som han hade hos sig, hörde detta,'),  # KGS1-011
    ('1Kgs', 1, 45): ('kommer det buller som I haven', 'kommer det buller, som I haven'),  # KGS1-012
    ('1Kgs', 1, 48): ("sagt så: 'Lovas vare HERREN, Israels Gud, som i dag har satt en efterträdare på min tron, så att jag med egna ögon har fått se det!»", "sagt så: 'Lovad vare HERREN, Israels Gud, som i dag har satt en efterträdare på min tron, så att jag med egna ögon har fått se det!'»"),  # KGS1-013
    ('1Kgs', 1, 49): ('alla de inbjudna som voro hos', 'alla de inbjudna, som voro hos'),  # KGS1-014
    ('1Kgs', 1, 51): ('i dag med ed att han icke', 'i dag med ed, att han icke'),  # KGS1-015
    ('1Kgs', 2, 1): ('tillstundade att David skulle', 'tillstundade, att David skulle'),  # KGS1-016
    ('1Kgs', 2, 3): ('Och håll vad HERREN,', 'Och håll, vad HERREN,'),  # KGS1-017
    ('1Kgs', 2, 4): ('uppfylla det ord som han talade', 'uppfylla det ord, som han talade'),  # KGS1-018
    ('1Kgs', 2, 8): ('förbannelser på den dag då jag gick', 'förbannelser på den dag, då jag gick'),  # KGS1-019
    ('1Kgs', 2, 9): ('man och vet väl vad du bör göra', 'man och vet väl, vad du bör göra'),  # KGS1-020
    ('1Kgs', 2, 10): ('och David gick', 'Och David gick'),  # KGS1-021
    ('1Kgs', 2, 12): ('tron, och han konungamakt', 'tron, och hans konungamakt'),  # KGS1-022
    ('1Kgs', 2, 15): ('»Du vet själv att konungadömet', '»Du vet själv, att konungadömet'),  # KGS1-023
    ('1Kgs', 2, 26): ('har fått lida.', 'har fått lida.»'),  # KGS1-024
    ('1Kgs', 2, 29): ('konung Salomo att Joab hade', 'konung Salomo, att Joab hade'),  # KGS1-025
    ('1Kgs', 2, 32): ('stötte ned två män som voro rättfärdigare', 'stötte ned två män, som voro rättfärdigare'),  # KGS1-026
    ('1Kgs', 2, 33): ('till evig tid.', 'till evig tid.»'),  # KGS1-027
    ('1Kgs', 2, 39): ('därefter hände sig att två tjänare flydde ifrån Simei till', 'därefter hände sig, att två tjänare flydde från Simei till'),  # KGS1-028
    ('1Kgs', 2, 41): ('berättat för Salomo att Simei hade', 'berättat för Salomo, att Simei hade'),  # KGS1-029
    ('1Kgs', 2, 44): ('allt det onda som ditt hjärta vet med sig att du har gjort', 'allt det onda, som ditt hjärta vet med sig, att du har gjort'),  # KGS1-030
    ('1Kgs', 3, 5): ('om vad du vill att jag skall', 'om vad du vill, att jag skall'),  # KGS1-031
    ('1Kgs', 3, 8): ('utvalt, ett folk som är så talrikt att det icke', 'utvalt, ett folk, som är så talrikt, att det icke'),  # KGS1-032
    ('1Kgs', 3, 13): ('giver jag dig ock vad du icke', 'giver jag dig ock, vad du icke'),  # KGS1-033
    ('1Kgs', 3, 15): ('Salomo och fann att det var', 'Salomo och fann, att det var'),  # KGS1-034
    ('1Kgs', 3, 21): ('di, fick jag se att han var död. Men när jag såg nogare på honom om morgonen, fick jag se att det icke', 'di, fick jag se, att han var död. Men när jag såg nogare på honom om morgonen, fick jag se, att det icke'),  # KGS1-035
    ('1Kgs', 3, 26): ('sade den kvinna vilkens son det levande barnet var till konungen', 'sade den kvinna, vilkens son det levande barnet var, till konungen'),  # KGS1-036
    ('1Kgs', 3, 28): ('konungen, ty de sågo att Guds vishet', 'konungen, ty de sågo, att Guds vishet'),  # KGS1-037
    ('1Kgs', 4, 19): ('land, det land som hade tillhört', 'land, det land, som hade tillhört'),  # KGS1-038
    ('1Kgs', 4, 23): ('och hundra far, förutom hjortar,', 'och hundra får, förutom hjortar,'),  # KGS1-039
    ('1Kgs', 4, 25): ('Så att Juda och', 'så att Juda och'),  # KGS1-040
    ('1Kgs', 5, 1): ('hade fått höra att denne hade', 'hade fått höra, att denne hade'),  # KGS1-041
    ('1Kgs', 5, 3): ('»Du vet själv att min fader David icke kunde bygga något hus åt HERRENS, sin Guds, namn, för de krigs skull med vilka fienderna runt omkring ansatte honom, till dess att HERREN lade dem under hans fötter', '»Du vet själv, att min fader David icke kunde bygga något hus åt HERRENS, sin Guds, namn, för de krigs skull, med vilka fienderna runt omkring ansatte honom, till dess att HERREN lade dem under hans fötter.'),  # KGS1-042
    ('1Kgs', 5, 6): ('Så bjud nu att man hugger åt mig cedrar på Libanon. Härvid skola mina tjänare vara dina tjänare behjälpliga; och jag vill giva dig betalning för dina tjänares arbete, alldeles såsom du själv begär. Ty du vet själv att bland oss', 'Så bjud nu, att man hugger åt mig cedrar på Libanon. Härvid skola mina tjänare vara dina tjänare behjälpliga; och jag vill giva dig betalning för dina tjänares arbete, alldeles såsom du själv begär. Ty du vet själv, att bland oss'),  # KGS1-043
    ('1Kgs', 5, 9): ('åter skall göra vad jag begär,', 'åter skall göra, vad jag begär,'),  # KGS1-044
    ('1Kgs', 5, 14): ('tio tusen i vår månad, skiftevis, så att de', 'tio tusen i var månad, skiftesvis, så att de'),  # KGS1-045
    ('1Kgs', 5, 15): ('sjuttio tusen män som buro bördor,', 'sjuttio tusen män, som buro bördor,'),  # KGS1-046
    ('1Kgs', 5, 17): ('med huggen sten,', 'med huggen sten.'),  # KGS1-047
    ('1Kgs', 5, 18): ('och de stenar som behövdes', 'och de stenar, som behövdes'),  # KGS1-048
    ('1Kgs', 6, 2): ('Huset som konung Salomo byggde åt HERREN var sextio alnar', 'Huset, som konung Salomo byggde åt HERREN, var sextio alnar'),  # KGS1-049
    ('1Kgs', 6, 6): ('omkring utvändigt, för att icke', 'omkring utvändigt för att icke'),  # KGS1-050
    ('1Kgs', 6, 22): ('hela det altare som hörde till', 'hela det altare, som hörde till'),  # KGS1-051
    ('1Kgs', 6, 27): ('keruberna i de innersta av', 'keruberna i det innersta av'),  # KGS1-052
    ('1Kgs', 6, 35): ('blomsterband, och överdrog', 'blomsterband och överdrog'),  # KGS1-053
    ('1Kgs', 6, 38): ('elfte året, månaden Bul, det är den åttonde månaden, var huset färdigt till alla sina delar alldeles såsom', 'elfte året, i månaden Bul, det är den åttonde månaden, var huset färdigt till alla sina delar, alldeles såsom'),  # KGS1-054
    ('1Kgs', 7, 6): ('förhus med pelare, och med ett', 'förhus med pelare och med ett'),  # KGS1-055
    ('1Kgs', 7, 8): ('andra gården, innanför förhuset,', 'andra gården innanför förhuset,'),  # KGS1-056
    ('1Kgs', 7, 19): ('pelarhuvudena som sutto ovanpå pelarna inne i förhuset voro utformade till liljor, och mätte fyra', 'pelarhuvudena, som sutto ovanpå pelarna inne i förhuset voro utformade till liljor och mätte fyra'),  # KGS1-057
    ('1Kgs', 7, 31): ('en aln uppåt. öppningen i detta', 'en aln uppåt. Öppningen i detta'),  # KGS1-058
    ('1Kgs', 7, 35): ('dess sidolister gjorda i ett', 'dess sidolister, gjorda i ett'),  # KGS1-059
    ('1Kgs', 7, 37): ('och samma form', 'och samma form.'),  # KGS1-060
    ('1Kgs', 7, 41): ('nämligen två pelare, och de två klotformiga pelarhuvuden som sutto ovanpå pelarna, och de två nätverk som skulle betäcka de båda klotformiga pelarhuvuden som sutto ovanpå', 'nämligen två pelare och de två klotformiga pelarhuvuden, som sutto ovanpå pelarna, och de två nätverk, som skulle betäcka de båda klotformiga pelarhuvuden, som sutto ovanpå'),  # KGS1-061
    ('1Kgs', 7, 42): ('pelarhuvuden som sutto uppe på pelarna så skulle bliva', 'pelarhuvuden, som sutto uppe på pelarna, så skulle bliva'),  # KGS1-062
    ('1Kgs', 7, 43): ('Vidare de tio', 'vidare de tio'),  # KGS1-063
    ('1Kgs', 7, 45): ('nämnda föremål som Hiram gjorde', 'nämnda föremål, som Hiram gjorde'),  # KGS1-064
    ('1Kgs', 7, 48): ('övriga föremål som skulle finnas i HERRENS hus: det gyllene altaret, det gyllene bordet som skådebröden', 'övriga föremål, som skulle finnas i HERRENS hus: det gyllene altaret, det gyllene bordet, som skådebröden'),  # KGS1-065
    ('1Kgs', 7, 51): ('allt det arbete som konung Salomo', 'allt det arbete, som konung Salomo'),  # KGS1-066
    ('1Kgs', 8, 7): ('över den plats där arken stod,', 'över den plats, där arken stod,'),  # KGS1-067
    ('1Kgs', 8, 9): ('två stentavlor som Mose hade', 'två stentavlor, som Mose hade'),  # KGS1-068
    ('1Kgs', 8, 12): ('»HERREN har sagt att han vill', '»HERREN har sagt, att han vill'),  # KGS1-069
    ('1Kgs', 8, 13): ('berett en plats där du må förbliva', 'berett en plats, där du må förbliva'),  # KGS1-070
    ('1Kgs', 8, 15): ('har fullbordat vad han med', 'har fullbordat, vad han med'),  # KGS1-071
    ('1Kgs', 8, 16): ("'Från den dag då jag förde mitt folk Israel ut ur Egypten har jag icke i någon av Israels stammar utvalt en stad, till att i den bygga ett hus där mitt namn", "'Från den dag, då jag förde mitt folk Israel ut ur Egypten, har jag icke i någon av Israels stammar utvalt en stad, till att i den bygga ett hus, där mitt namn"),  # KGS1-072
    ('1Kgs', 8, 20): ('har kommit upp min fader Davids', 'har kommit upp i min fader Davids'),  # KGS1-073
    ('1Kgs', 8, 21): ('förvaras det förbund som HERREN slöt', 'förvaras det förbund, som HERREN slöt'),  # KGS1-074
    ('1Kgs', 8, 24): ('som har hållit vad du lovade', 'som har hållit, vad du lovade'),  # KGS1-075
    ('1Kgs', 8, 26): ('Israels Gud, de ord som du har talat', 'Israels Gud, de ord, som du har talat'),  # KGS1-076
    ('1Kgs', 8, 27): ('då detta hus som jag har', 'då detta hus, som jag har'),  # KGS1-077
    ('1Kgs', 8, 29): ("hus – den plats varom du har sagt: 'Mitt namn skall vara där' – så att du ock hör den bön som din tjänare", "hus – den plats, varom du har sagt: 'Mitt namn skall vara där' – så att du ock hör den bön, som din tjänare"),  # KGS1-078
    ('1Kgs', 8, 36): ('den goda väg som de skola', 'den goda väg, som de skola'),  # KGS1-079
    ('1Kgs', 8, 37): ('folket i det land där deras städer', 'folket i det land, där deras städer'),  # KGS1-080
    ('1Kgs', 8, 41): ('ditt namns skull', 'ditt namns skull –'),  # KGS1-081
    ('1Kgs', 8, 42): ('– ty man skall', 'ty man skall'),  # KGS1-082
    ('1Kgs', 8, 43): ('och göra allt varom främlingen ropar till dig, på det att alla jordens folk må känna ditt namn och frukta dig, likasom ditt folk Israel gör, och förnimma att detta hus som jag har byggt är uppkallat', 'och göra allt, varom främlingen ropar till dig, på det att alla jordens folk må känna ditt namn och frukta dig, likasom ditt folk Israel gör, och förnimma, att detta hus, som jag har byggt, är uppkallat'),  # KGS1-083
    ('1Kgs', 8, 47): ('sig i det land där de äro i fångenskap, och omvända sig och åkalla dig i landet där man håller', 'sig i det land, där de äro i fångenskap, och omvända sig och åkalla dig i landet, där man håller'),  # KGS1-084
    ('1Kgs', 8, 50): ('förlåta ditt folk vad de hava syndat mot dig, och alla de överträdelser som de hava', 'förlåta ditt folk, vad de hava syndat mot dig, och alla de överträdelser, som de hava'),  # KGS1-085
    ('1Kgs', 8, 53): ('alla folk på Jorden, såsom', 'alla folk på jorden, såsom'),  # KGS1-086
    ('1Kgs', 8, 56): ('allt det goda som han lovade', 'allt det goda, som han lovade'),  # KGS1-087
    ('1Kgs', 8, 60): ('jorden må förnimma att HERREN är Gud, och ingen annan.', 'jorden må förnimma, att HERREN är Gud och ingen annan.'),  # KGS1-088
    ('1Kgs', 8, 64): ('var för litet för att brännoffret,', 'var för litet, för att brännoffret,'),  # KGS1-089
    ('1Kgs', 8, 65): ('ifrån det ställe där vägen går', 'ifrån det ställe, där vägen går'),  # KGS1-090
    ('1Kgs', 9, 3): ('sade till honom »Jag har hört den bön och åkallan som du har uppsänt till mig; detta hus som du har byggt har jag helgat,', 'sade till honom: »Jag har hört den bön och åkallan, som du har uppsänt till mig; detta hus, som du har byggt, har jag helgat,'),  # KGS1-091
    ('1Kgs', 9, 4): ('stadgar och rätter', 'stadgar och rätter,'),  # KGS1-092
    ('1Kgs', 9, 6): ('övergiven mig, och icke hållen de bud och stadgar som jag har', 'övergiven mig och icke hållen de bud och stadgar, som jag har'),  # KGS1-093
    ('1Kgs', 9, 7): ('dem; och det hus som jag har helgat åt mitt namn skall jag förkasta', 'dem; och det hus, som jag har helgat åt mitt namn, skall jag förkasta'),  # KGS1-094
    ('1Kgs', 9, 12): ('bese de städer som Salomo hade', 'bese de städer, som Salomo hade'),  # KGS1-095
    ('1Kgs', 9, 13): ('detta för städer som du har givit', 'detta för städer, som du har givit'),  # KGS1-096
    ('1Kgs', 9, 16): ('dräpt de kananéer som bodde i', 'dräpt de kananéer, som bodde i'),  # KGS1-097
    ('1Kgs', 9, 20): ('Allt det folk som fanns kvar av amoréerna, hetiterna, perisséerna hivéerna och', 'Allt det folk, som fanns kvar av amoréerna, hetiterna, perisséerna, hivéerna och'),  # KGS1-098
    ('1Kgs', 9, 22): ('tjänare och hövitsman och kämpar,', 'tjänare och hövitsmän och kämpar,'),  # KGS1-099
    ('1Kgs', 10, 4): ('Salomos vishet, och såg huset som han hade', 'Salomos vishet och såg huset, som han hade'),  # KGS1-100
    ('1Kgs', 10, 5): ('på hans bord, och såg huru hans tjänare sutto där, och huru de som betjänade honom utförde sina åligganden, och huru de voro klädda, och vidare såg hans munskänkar, och när hon såg brännoffren som han offrade', 'på hans bord och såg, huru hans tjänare sutto där, och huru de som betjänade honom utförde sina åligganden, och huru de voro klädda, och vidare såg hans munskänkar, och när hon såg brännoffren, som han offrade'),  # KGS1-101
    ('1Kgs', 10, 6): ('»Sant var det tal som jag hörde', '»Sant var det tal, som jag hörde'),  # KGS1-102
    ('1Kgs', 10, 7): ('ville icke tro vad man sade, förrän jag själv kom och med egna ögon fick se det; men nu finner jag att det icke', 'ville icke tro, vad man sade, förrän jag själv kom och med egna ögon fick se det; men nu finner jag, att det icke'),  # KGS1-103
    ('1Kgs', 10, 14): ('Det guld som årligen inkom till Salomo vägde sex hundra', 'Det guld, som årligen inkom till Salomo, vägde sex hundra'),  # KGS1-104
    ('1Kgs', 10, 24): ('höra den vishet som Gud hade', 'höra den vishet, som Gud hade'),  # KGS1-105
    ('1Kgs', 10, 28): ('Och hästarna som Salomo lät anskaffa infördes från', 'Och hästarna, som Salomo lät anskaffa, infördes från'),  # KGS1-106
    ('1Kgs', 10, 29): ('Var vagn som hämtades upp från Egypten och infördes kostade sex', 'Var vagn, som hämtades upp från Egypten och infördes, kostade sex'),  # KGS1-107
    ('1Kgs', 11, 1): ('utländska kvinnor som han älskade:', 'utländska kvinnor, som han älskade:'),  # KGS1-108
    ('1Kgs', 11, 2): ('kvinnor av de folk om vilka HERREN hade lagt till Israels', 'kvinnor av de folk, om vilka HERREN hade sagt till Israels'),  # KGS1-109
    ('1Kgs', 11, 10): ('ett HERRENS bud som han icke', 'ett HERRENS bud, som han icke'),  # KGS1-110
    ('1Kgs', 11, 11): ('och de stadgar som jag har', 'och de stadgar, som jag har'),  # KGS1-111
    ('1Kgs', 11, 18): ('De begav sig åstad från', 'De begåvo sig åstad från'),  # KGS1-112
    ('1Kgs', 11, 21): ('Egypten hörde att David hade', 'Egypten hörde, att David hade'),  # KGS1-113
    ('1Kgs', 11, 22): ('utan låt mig gå.', 'utan låt mig gå.»'),  # KGS1-114
    ('1Kgs', 11, 27): ('Orsaken varför han reste sig upp mot konungen var följande.', 'Orsaken, varför han reste sig upp mot konungen, var följande.'),  # KGS1-115
    ('1Kgs', 11, 28): ('då Salomo såg att den unge', 'då Salomo såg, att den unge'),  # KGS1-116
    ('1Kgs', 11, 29): ('hände sig en gång att Jerobeam', 'hände sig en gång, att Jerobeam'),  # KGS1-117
    ('1Kgs', 11, 32): ('skull, den stads som jag har', 'skull, den stads, som jag har'),  # KGS1-118
    ('1Kgs', 11, 36): ('Jerusalem, den stad som jag har', 'Jerusalem, den stad, som jag har'),  # KGS1-119
    ('1Kgs', 11, 38): ('åt dig ett hus som bliver beståndande,', 'åt dig ett hus, som bliver beståndande,'),  # KGS1-120
    ('1Kgs', 12, 4): ('det tunga ok som din fader', 'det tunga ok, som din fader'),  # KGS1-121
    ('1Kgs', 12, 9): ("giva detta folk som har talat till mig och sagt: 'Lätta det ok som din fader", "giva detta folk, som har talat till mig och sagt: 'Lätta det ok, som din fader"),  # KGS1-122
    ('1Kgs', 12, 10): ('De unga männen som hade vuxit upp med honom svarade honom', 'De unga männen, som hade vuxit upp med honom, svarade honom'),  # KGS1-123
    ('1Kgs', 12, 16): ('Israel förnam att konungen', 'Israel förnam, att konungen'),  # KGS1-124
    ('1Kgs', 12, 20): ('Israel hörde att Jerobeam', 'Israel hörde, att Jerobeam'),  # KGS1-125
    ('1Kgs', 12, 32): ('lik högtiden Juda, och steg då upp till altaret; så gjorde han i Betel för att offra åt de kalvar som han hade låtit göra. Och de män som han hade gjort till offerhöjdspräster lät han göra', 'lik högtiden i Juda, och steg då upp till altaret; så gjorde han i Betel för att offra åt de kalvar som han hade låtit göra. Och de män, som han hade gjort till offerhöjdspräster, lät han göra'),  # KGS1-126
    ('1Kgs', 12, 33): ('månaden, den månad som han av eget', 'månaden, den månad, som han av eget'),  # KGS1-127
    ('1Kgs', 13, 2): ('vid namn Josia, han skall på dig slakta offerhöjdsprästerna som antända', 'vid namn Josia; han skall på dig slakta offerhöjdsprästerna, som antända'),  # KGS1-128
    ('1Kgs', 13, 3): ('det är HERREN som har talat:', 'det är HERREN, som har talat:'),  # KGS1-129
    ('1Kgs', 13, 4): ('honom.» Men handen som han hade räckt ut mot honom förvissnade,', 'honom.» Men handen, som han hade räckt ut mot honom, förvissnade,'),  # KGS1-130
    ('1Kgs', 13, 5): ('var det tecken som gudsmannen', 'var det tecken, som gudsmannen'),  # KGS1-131
    ('1Kgs', 13, 6): ('och bed för mig att jag må kunna', 'och bed för mig, att jag må kunna'),  # KGS1-132
    ('1Kgs', 13, 9): ('eller dricka, och ej heller', 'eller dricka och ej heller'),  # KGS1-133
    ('1Kgs', 13, 12): ('deras fader dem vilken väg han hade gått. Och hans söner visste vilken väg gudsmannen som kom från Juda hade gått.', 'deras fader dem, vilken väg han hade gått. Och hans söner visste, vilken väg gudsmannen, som kom från Juda, hade gått.'),  # KGS1-134
    ('1Kgs', 13, 14): ('och begav dig åstad efter gudsmannen och fann honom sittande under terebinten; och han frågade honom: »Är du den gudsman som har kommit', 'och begav sig åstad efter gudsmannen och fann honom sittande under terebinten; och han frågade honom: »Är du den gudsman, som har kommit'),  # KGS1-135
    ('1Kgs', 13, 21): ('hållit det bud som HERREN,', 'hållit det bud, som HERREN,'),  # KGS1-136
    ('1Kgs', 13, 22): ('druckit på den ort där han hade förbjudit, dig att äta och dricka, därför skall din döda kropp icke komma i dina fäders grav.', 'druckit på den ort, där han hade förbjudit dig att äta och dricka, därför skall din döda kropp icke komma i dina fäders grav.»'),  # KGS1-137
    ('1Kgs', 13, 25): ('Då nu folk som gick därförbi fick se den döda kroppen ligga utsträckt på vägen och lejonet stå bredvid den döda kroppen, gingo de in i staden där den gamle profeten bodde och omtalade', 'Då nu folk, som gick därförbi, fick se den döda kroppen ligga utsträckt på vägen och lejonet stå bredvid den döda kroppen, gingo de in i staden, där den gamle profeten bodde, och omtalade'),  # KGS1-138
    ('1Kgs', 13, 27): ('han sina söner att de skulle', 'han sina söner, att de skulle'),  # KGS1-139
    ('1Kgs', 13, 31): ('mig i den grav där gudsmannen', 'mig i den grav, där gudsmannen'),  # KGS1-140
    ('1Kgs', 13, 33): ('åter allahanda man ur folket till', 'åter allahanda män ur folket till'),  # KGS1-141
    ('1Kgs', 13, 34): ('orsak till synd, och en orsak', 'orsak till synd och en orsak'),  # KGS1-142
    ('1Kgs', 14, 2): ('ingen kan märka att du är Jerobeams hustru, och gå till Silo, ty där bor profeten Ahia, han som förkunnade om mig att jag skulle', 'ingen kan märka, att du är Jerobeams hustru, och gå till Silo, ty där bor profeten Ahia, han som förkunnade om mig, att jag skulle'),  # KGS1-143
    ('1Kgs', 14, 3): ('förkunna för dig huru det skall', 'förkunna för dig, huru det skall'),  # KGS1-144
    ('1Kgs', 14, 5): ('sig främmande.', 'sig främmande.»'),  # KGS1-145
    ('1Kgs', 14, 11): ('ute på marken, skola himmelens', 'ute på marken skola himmelens'),  # KGS1-146
    ('1Kgs', 14, 14): ('sig, en konung som skall utrota', 'sig, en konung, som skall utrota'),  # KGS1-147
    ('1Kgs', 14, 16): ('synders skull som Jerobeam', 'synders skull, som Jerobeam'),  # KGS1-148
    ('1Kgs', 14, 21): ('Jerusalem, den stad som HERREN hade', 'Jerusalem, den stad, som HERREN hade'),  # KGS1-149
    ('1Kgs', 14, 26): ('gyllene sköldar som Salomo hade', 'gyllene sköldar, som Salomo hade'),  # KGS1-150
    ('1Kgs', 15, 11): ('David hade gjort', 'David hade gjort.'),  # KGS1-151
    ('1Kgs', 15, 12): ('avgudabeläten som hans fader hade låtit', 'avgudabeläten, som hans fäder hade låtit'),  # KGS1-152
    ('1Kgs', 15, 17): ('befästa Rama, för att hindra att någon komme', 'befästa Rama för att hindra, att någon komme'),  # KGS1-153
    ('1Kgs', 15, 18): ('silver och guld som fanns kvar', 'silver och guld, som fanns kvar'),  # KGS1-154
    ('1Kgs', 15, 19): ('silver och guld, så bryt då nu', 'silver och guld; så bryt då nu'),  # KGS1-155
    ('1Kgs', 15, 22): ('och trävirke som Baesa använde', 'och trävirke, som Baesa använde'),  # KGS1-156
    ('1Kgs', 15, 26): ('och i den synd genom vilken', 'och i den synd, genom vilken'),  # KGS1-157
    ('1Kgs', 15, 27): ('son, av Isaskar hus, anstiftade', 'son, av Isaskars hus, anstiftade'),  # KGS1-158
    ('1Kgs', 15, 29): ('blivit konung förgjorde han hela Jerobeams hus; han lät intet som anda hade bliva kvar av Jerobeams hus, utan utrotade det, i enighet med det', 'blivit konung, förgjorde han hela Jerobeams hus; han lät intet som anda hade bliva kvar av Jerobeams hus, utan utrotade det, i enlighet med det'),  # KGS1-159
    ('1Kgs', 15, 31): ('vad han gjorde det finnes upptecknat', 'vad han gjorde, det finnes upptecknat'),  # KGS1-160
    ('1Kgs', 15, 32): ('med varandra, länge de levde.', 'med varandra, så länge de levde.'),  # KGS1-161
    ('1Kgs', 15, 34): ('och i den synd genom vilken', 'och i den synd, genom vilken'),  # KGS1-162
    ('1Kgs', 16, 13): ('synders skull som Baesa och', 'synders skull, som Baesa och'),  # KGS1-163
    ('1Kgs', 16, 16): ('de höra sägas »Simri har anstiftat', 'de höra sägas: »Simri har anstiftat'),  # KGS1-164
    ('1Kgs', 16, 18): ('när Simri såg att staden var', 'när Simri såg, att staden var'),  # KGS1-165
    ('1Kgs', 16, 22): ('del av folket som höll sig', 'del av folket, som höll sig'),  # KGS1-166
    ('1Kgs', 16, 24): ('kallade staden som han byggde där Samaria, efter Semer, den man som hade varit', 'kallade staden, som han byggde där, Samaria, efter Semer, den man, som hade varit'),  # KGS1-167
    ('1Kgs', 16, 26): ('och i de synder genom vilka', 'och i de synder, genom vilka'),  # KGS1-168
    ('1Kgs', 17, 10): ('där se en änka som samlade', 'där se en änka, som samlade'),  # KGS1-169
    ('1Kgs', 17, 12): ('allenast en hand full mjöl i krukan', 'allenast en handfull mjöl i krukan'),  # KGS1-170
    ('1Kgs', 17, 19): ('honom upp i salen där han bodde och lade honom', 'honom upp i salen, där han bodde, och lade honom'),  # KGS1-171
    ('1Kgs', 17, 24): ('Elia: »Nu vet jag att du är en', 'Elia: »Nu vet jag, att du är en'),  # KGS1-172
    ('1Kgs', 18, 6): ('mellan sig landet som de skulle draga i genom. Ahab for', 'mellan sig landet, som de skulle draga igenom. Ahab for'),  # KGS1-173
    ('1Kgs', 18, 7): ('föll ned på Sitt ansikte och', 'föll ned på sitt ansikte och'),  # KGS1-174
    ('1Kgs', 18, 10): ('eller något rike dit min herre', 'eller något rike, dit min herre'),  # KGS1-175
    ('1Kgs', 18, 13): ('för min herre vad jag gjorde,', 'för min herre, vad jag gjorde,'),  # KGS1-176
    ('1Kgs', 18, 20): ('omkring bland Israels barn', 'omkring bland alla Israels barn'),  # KGS1-177
    ('1Kgs', 18, 22): ('såsom HERRENS Profet, och Baals', 'såsom HERRENS profet, och Baals'),  # KGS1-178
    ('1Kgs', 18, 24): ('HERRENS namn. »Den gud som då svarar med eld, han vare Gud.» Allt folket svarade och sade. »Ditt förslag', 'HERRENS namn. Den gud, som då svarar med eld, han vare Gud.» Allt folket svarade och sade: »Ditt förslag'),  # KGS1-179
    ('1Kgs', 18, 26): ('togo de den tjur som han gav', 'togo de den tjur, som han gav'),  # KGS1-180
    ('1Kgs', 18, 29): ('profetiskt raseri, och höllo så på ända till den tid då spisoffret', 'profetiskt raseri och höllo så på ända till den tid, då spisoffret'),  # KGS1-181
    ('1Kgs', 18, 36): ('bliva kunnigt att du är Gud', 'bliva kunnigt, att du är Gud'),  # KGS1-182
    ('1Kgs', 18, 37): ('folk förnimmer att det är du,', 'folk förnimmer, att det är du,'),  # KGS1-183
    ('1Kgs', 18, 38): ('»Då föll HERRENS ed ned och förtärde brännoffret, veden, stenarna och jorden, och uppslickade vattnet som var i graven.', 'Då föll HERRENS eld ned och förtärde brännoffret, veden, stenarna och jorden och uppslickade vattnet, som var i graven.'),  # KGS1-184
    ('1Kgs', 18, 43): ('till sin tjänare »Gå upp och', 'till sin tjänare: »Gå upp och'),  # KGS1-185
    ('1Kgs', 18, 44): ('sjunde gången sade han: »Nu', 'sjunde gången, sade han: »Nu'),  # KGS1-186
    ('1Kgs', 19, 2): ('Sände Isebel en budbärare till Elia och lät säga: »Gudarna straffe mig nu och framgent om jag icke', 'sände Isebel en budbärare till Elia och lät säga: »Gudarna straffe mig nu och framgent, om jag icke'),  # KGS1-187
    ('1Kgs', 19, 18): ('alla de knän som icke hava böjt sig för Baal, och var mun som icke har', 'alla de knän, som icke hava böjt sig för Baal, och var mun, som icke har'),  # KGS1-188
    ('1Kgs', 19, 20): ('igen; du vet ju vad jag har', 'igen; du vet ju, vad jag har'),  # KGS1-189
    ('1Kgs', 20, 7): ('»Märken och sen huru denne står', '»Märken och sen, huru denne står'),  # KGS1-190
    ('1Kgs', 20, 13): ('hela denna stor hop? Se, jag vill i dag giva den i din hand, på det att du må förnimma att jag är HERREN.»', 'hela denna stora hop? Se, jag vill i dag giva den i din hand, på det att du må förnimma, att jag är HERREN.»'),  # KGS1-191
    ('1Kgs', 20, 14): ('Ahab: »Genom vem? Han svarade:', 'Ahab: »Genom vem?» Han svarade:'),  # KGS1-192
    ('1Kgs', 20, 15): ('hundra trettiotvå, därefter mönstrade', 'hundra trettiotvå; därefter mönstrade'),  # KGS1-193
    ('1Kgs', 20, 16): ('konungar som sade kommit honom', 'konungar som hade kommit honom'),  # KGS1-194
    ('1Kgs', 20, 17): ('de kunskapare som Ben-Hadad sände ut underrättade', 'de kunskapare, som Ben-Hadad sände ut, underrättade'),  # KGS1-195
    ('1Kgs', 20, 19): ('män och hären som följde dem', 'män och hären, som följde dem'),  # KGS1-196
    ('1Kgs', 20, 28): ('mån förnimma att jag är HERREN.»', 'mån förnimma, att jag är HERREN.»'),  # KGS1-197
    ('1Kgs', 20, 31): ('»Vi hava hört att konungarna', '»Vi hava hört, att konungarna'),  # KGS1-198
    ('1Kgs', 20, 34): ('honom: »De städer som min fader tog från din fader vill jag giva', 'honom: »De städer, som min fader tog från din fader, vill jag giva'),  # KGS1-199
    ('1Kgs', 20, 41): ('honom och såg att han var', 'honom och såg, att han var'),  # KGS1-200
    ('1Kgs', 20, 42): ('hand den man som av mig var', 'hand den man, som av mig var'),  # KGS1-201
    ('1Kgs', 21, 4): ('det svars skull som jisreeliten', 'det svars skull, som jisreeliten'),  # KGS1-202
    ('1Kgs', 21, 6): ("min vingård.'", "min vingård.'»"),  # KGS1-203
    ('1Kgs', 21, 7): ('hustru Isebel honom: »Är det du som nu regerar över Israel? Stå upp och ät och var vid gott mod; jag skall skaffa dig jisreeliten Nabot vingård.', 'hustru Isebel till honom: »Är det du, som nu regerar över Israel? Stå upp och ät och var vid gott mod; jag skall skaffa dig jisreeliten Nabots vingård.»'),  # KGS1-204
    ('1Kgs', 21, 13): ('folket och sade: Nabot har talat', 'folket och sade: »Nabot har talat'),  # KGS1-205
    ('1Kgs', 21, 15): ('Isebel hörde att Nabot var', 'Isebel hörde, att Nabot var'),  # KGS1-206
    ('1Kgs', 21, 16): ('snart Ahab hörde att Nabot var', 'snart Ahab hörde, att Nabot var'),  # KGS1-207
    ('1Kgs', 21, 19): ("HERREN: Har du till redan hunnit att både dräpa och tillträda arvet?' Därefter skall du tala till honom och säga: 'Så säger HERREN: På samma ställe där hundarna hava slickat Nabots blod skola hundarna", "HERREN: Har du redan hunnit att både dräpa och tillträda arvet?' Därefter skall du tala till honom och säga: 'Så säger HERREN: På samma ställe, där hundarna hava slickat Nabots blod, skola hundarna"),  # KGS1-208
    ('1Kgs', 21, 29): ('»Har du sett huru Ahab ödmjukar', '»Har du sett, huru Ahab ödmjukar'),  # KGS1-209
    ('1Kgs', 22, 3): ('tjänare: »I veten ju att Ramot i', 'tjänare: »I veten ju, att Ramot i'),  # KGS1-210
    ('1Kgs', 22, 4): ('frågade Josafat: Vill du draga', 'frågade Josafat: »Vill du draga'),  # KGS1-211
    ('1Kgs', 22, 13): ('Och budet som hade gått för att tillkalla Mika talade till', 'Och budet, som hade gått för att tillkalla Mika, talade till'),  # KGS1-212
    ('1Kgs', 22, 18): ('jag dig icke att denne aldrig', 'jag dig icke, att denne aldrig'),  # KGS1-213
    ('1Kgs', 22, 25): ('det på den dag då du nödgas', 'det på den dag, då du nödgas'),  # KGS1-214
    ('1Kgs', 22, 33): ('vagnarna märkte att det icke', 'vagnarna märkte, att det icke'),  # KGS1-215
    ('1Kgs', 22, 34): ('Men en man som spände sin båge och sköt på måfå träffade Israels', 'Men en man, som spände sin båge och sköt på måfå, träffade Israels'),  # KGS1-216
    ('1Kgs', 22, 41): ('Josafat, Asas sons blev konung', 'Josafat, Asas son, blev konung'),  # KGS1-217
    ('1Kgs', 22, 47): ('tempelbolare som ännu funnos', 'tempelbolare, som ännu funnos'),  # KGS1-218
}
