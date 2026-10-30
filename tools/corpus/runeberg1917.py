"""Read the 66 canonical books of Bibeln 1917 from Project Runeberg's single-file e-text.

The e-text keeps the 1917 verse numbers. Chapter headings follow older divisions,
so a verse can appear before or after the heading of the next chapter; its number
decides where it belongs. Summaries, cross references and footnotes are not Scripture.
"""
import html, re, zipfile
# Runeberg book numbers 1-66 follow the 1917 order; Hebrews to Jude differ from the KJV order.
CODES=['Gen','Exod','Lev','Num','Deut','Josh','Judg','Ruth','1Sam','2Sam','1Kgs','2Kgs','1Chr','2Chr','Ezra','Neh','Esth','Job','Ps','Prov','Eccl','Song','Isa','Jer','Lam','Ezek','Dan','Hos','Joel','Amos','Obad','Jonah','Micah','Nah','Hab','Zeph','Hag','Zech','Mal',
 'Matt','Mark','Luke','John','Acts','Rom','1Cor','2Cor','Gal','Eph','Phil','Col','1Thess','2Thess','1Tim','2Tim','Titus','Philemon','Heb','1Peter','2Peter','1John','2John','3John','James','Jude','Rev']
SINGLE={'31':'Obadja','57':"Paulus' brev till Filemon",'62':"Johannes' andra brev",'63':"Johannes' tredje brev",'65':"Judas' brev"}
TITLE=re.compile(r'^\s+\S.*, \d+ (?:Kapitlet|Psalmen)(?:\[\d+\])?$')
DIVISION=re.compile(r'^\s+(?:Första|Andra|Tredje|Fjärde|Femte) boken$')
VERSE=re.compile(r'^\s{0,3}(\d+)\.\s(.*)$')
# Placeholder for verses printed in another chapter, such as "6:1.  [1]".
ELSEWHERE=re.compile(r'^\s*\d+:\d+\.\s*(?:\[\d+\])?$')
# Misprinted verse numbers in the e-text: (book, chapter, printed number, occurrence) -> number.
RENUMBER={('25',1,21,2):22,('42',20,23,1):22}
# E-text errors checked visually against the 1927 facsimile.
# Page links, context and pending human review: docs/reports/sv1917-errata.md.
REPLACE={
 ('2Kgs',18,21):('förtröstar val nu','förtröstar väl nu'),
 ('Job',9,35):('med min själv','med mig själv'),
 ('Ps',22,30):('sin själv vid liv','sin själ vid liv'),
 ('Prov',20,7):('går det val efter honom','går det väl efter honom'),
 ('Prov',23,7):('kan han val säga','kan han väl säga'),
 ('Jer',8,12):('på skam, övade styggelse. Likväl känna de alls icke skam','på skam, ty de övade styggelse. Likväl känna de alls icke någon skam'), # R006 + R023, photo V001
 ('Jer',8,15):('V bida efter frid','Vi bida efter frid'),
 ('Jer',16,13):('ett land son varken','ett land som varken'),
 ('Jer',17,21):('Tagen eder val till vara','Tagen eder väl till vara'),
 ('Jer',20,13):('den fattiges själv','den fattiges själ'),
 ('Jer',29,7):('så går det ock eder val','så går det ock eder väl'),
 ('Ezek',3,17):('å mina vagnar','å mina vägnar'),
 ('Luke',12,48):('vad som val slag värt','vad som var slag värt'),
 ('Ps',44,11):('för ovånnen','för ovännen'),
 ('Ps',66,14):('mina låppar','mina läppar'),
 ('Ps',102,27):('skall förvanda dem','skall förvandla dem'),
 ('Prov',12,22):('lögnaktiga låppar','lögnaktiga läppar'),
 ('Hag',1,1):('profeten Haggai Serubbabel','profeten Haggai till Serubbabel'),
 ('Hag',1,6):('han far den allenast','han får den allenast'),
 ('Hag',1,14):('HERREN Sebaots; sin Guds','HERREN Sebaots, sin Guds'),
 ('Hag',2,2):('profeten Haggai han sade','profeten Haggai; han sade'),
 ('Hag',2,16):('till HERRENS tempel','till HERRENS tempel;'),
 # Deuteronomy 1
 ('Deut',1,3):('Herren','HERREN'),
 ('Deut',1,5):('.',','),
 ('Deut',1,15):('och somlig','och andr'),
 ('Deut',1,20):('HERREN','HERREN,'),
 ('Deut',1,22):(', angående vägen',' angående vägen,'),
 ('Deut',1,25):('Herren, vår Gud, vill giva oss','HERREN, vår Gud, vill giva oss,'),
 ('Deut',1,33):('vägen,','vägen'),
 ('Deut',1,35):('land','land,'),
 ('Deut',1,36):('Herren','HERREN'),
 ('Deut',1,37):('skull,','skull'),
 ('Deut',1,39):('byte, och ','byte, och edra söner, som ännu icke förstå vare sig gott eller ont, de '),
 ('Deut',1,44):(' som bodde där i bergsbygden drogo',', som bodde där i bergsbygden drogo ut'),
 # Deuteronomy 2
 ('Deut',2,1):('Herren','HERREN'),
 ('Deut',2,9):('Herren','HERREN'),
 ('Deut',2,14):('. var trettioåtta år, och under denna tid förgicks hela den släktet, alla stridbara män i lägret, såsom Herren',', var trettioåtta år, och under denna tid förgicks hela det släktet, alla stridbara män i lägret, såsom HERREN'),
 ('Deut',2,35):(',',''),
 ('Deut',2,36):('ingen stad','ingen stad,'),
 ('Deut',2,37):(', och städerna i bergsbygden, och allt övrigt',' och städerna i bergsbygden och allt övrigt,'),
 # Deuteronomy 3
 ('Deut',3,1):('ut i','ut till'),
 ('Deut',3,2):('och h','och hans land. Och du skall göra med h'),
 ('Deut',3,11):('ende','ende,'),
 ('Deut',3,17):('till ','till Hedmarkshavet, Salthavet, nedanför '),
 ('Deut',3,20):(', såväl som eder, när också de hava tagit i besittning det land',' såväl som eder, när också de hava tagit i besittning det land,'),
 ('Deut',3,21):('riken','riken,'),
 ('Deut',3,24):('låta s','låta d'),
 ('Deut',3,28):(' som skall gå ditöver i spetsen för detta folk, och det är han',', som skall gå ditöver i spetsen för detta folk, och det är han,'),
 # Deuteronomy 4
 ('Deut',4,1):(' som jag vill lära eder, för att I mån göra efter dem, på det att I mån leva och komma in i och taga i besittning det land',', som jag vill lära eder, för att I mån göra efter dem, på det att I mån leva och komma in i och taga i besittning det land,'),
 ('Deut',4,3):('sett','sett,'),
 ('Deut',4,5):(' såsom HERREN, min Gud, har bjudit mig, på det att I mån göra efter dem i det land dit I nu kommen,',', såsom HERREN, min Gud, har bjudit mig, på det att I mån göra efter dem i det land, dit I nu kommen'),
 ('Deut',4,6):('skola ','skola de '),
 ('Deut',4,7):('nära','nära,'),
 ('Deut',4,9):('vika ifrån ditt hjärta i all','det vika ifrån ditt hjärta i alla'),
 ('Deut',4,10):(' då du stod inför HERREN, din Gud, vid Horeb, då HERREN sade till mig: »Församla folket till mig, för att jag må låta dem höra mina ord; må de så lära sig att frukta mig, så länge de leva på jorden, och',', då du stod inför HERREN, din Gud, vid Horeb, då HERREN sade till mig: »Församla folket till mig, för att jag må låta dem höra mina ord; må de så lära sig att frukta mig, så länge de leva på jorden, och må'),
 ('Deut',4,11):('blevo','bleven'),
 ('Deut',4,12):('elden','elden;'),
 ('Deut',4,13):('hålla','hålla,'),
 ('Deut',4,14):(' att jag skulle lära eder stadgar och rätter, för att I skullen göra efter dem i det land dit I nu dragen,',', att jag skulle lära eder stadgar och rätter, för att I skullen göra efter dem i det land, dit I nu dragen'),
 ('Deut',4,15):(' då HERREN talade till eder på Horeb ur elden, därför mån I nu noga hava akt på eder själva.',', då HERREN talade till eder på Horeb ur elden, därför mån I nu noga hava akt på eder själva,'),
 ('Deut',4,16):('någon beläte, något slags avgudabild, något','något beläte, något slags avgudabild, någon'),
 ('Deut',4,21):(', och svor att jag icke skulle få gå över Jordan och komma in i de',' och svor, att jag icke skulle få gå över Jordan och komma in i det'),
 ('Deut',4,23):('förbund','förbund,'),
 ('Deut',4,24):('Ty,','Ty'),
 ('Deut',4,25):('gören','gören,'),
 ('Deut',4,26):(' dit I nu gån över Jordan, för att taga det i besittning; I skolen då icke längr',', dit I nu gån över Jordan för att taga det i besittning; I skolen då icke läng'),
 ('Deut',4,27):('de folk','de folk,'),
 ('Deut',4,32):('andra','andra,'),
 ('Deut',4,34):(', genom hemsökelser, tecken och under, genom krig, genom stark hand och uträckt arm, och genom stora, fruktansvärda gärningar, vilket allt HERREN, eder Guds',' genom hemsökelser, tecken och under, genom krig, genom stark hand och uträckt arm och genom stora, fruktansvärda gärningar, vilket allt HERREN, eder Gud'),
 ('Deut',4,35):(' att HERREN är Gud,',', att HERREN är Gud'),
 ('Deut',4,37):('dem,','dem'),
 ('Deut',4,38):('folk','folk,'),
 ('Deut',4,39):('hjärtat','hjärtat,'),
 # Deuteronomy 5
 ('Deut',5,1):('rätter','rätter,'),
 ('Deut',5,5):('eder,','eder'),
 ('Deut',5,9):('HERREN, s','HERREN, d'),
 ('Deut',5,11):('Guds','Guds,'),
 ('Deut',5,14):('sin dotter, eller din tjänare eller din tjänarinna,','din dotter eller din tjänare eller din tjänarinna'),
 ('Deut',5,15):(' att du själv har varit',', att du själv har varit en'),
 ('Deut',5,16):('såsom HERREN, din Gud','såsom HERREN, din Gud,'),
 ('Deut',5,20):('falsk','falskt'),
 ('Deut',5,22):('berget,','berget'),
 ('Deut',5,24):('sett','sett,'),
 ('Deut',5,26):('kött','kött,'),
 ('Deut',5,28):('de ord','de ord,'),
 ('Deut',5,31):('förkunna för dig alla de bud och stadgar och rätter','jag förkunna för dig alla de bud och stadgar och rätter,'),
 ('Deut',5,32):('gören','gören,'),
 # Deuteronomy 6
 ('Deut',6,1):(' som HERREN, eder Gud, har bjudit mig att lära eder, för att I skolen göra efter dem i det land dit I nu dragen, till att taga de',', som HERREN, eder Gud, har bjudit mig att lära eder, för att I skolen göra efter dem i det land, dit I nu dragen till att taga det'),
 ('Deut',6,2):('som och din sonson, i all','son och din sonson, i alla'),
 ('Deut',6,6):('dig','dig,'),
 ('Deut',6,17):('hat','har'),
 ('Deut',6,18):('göra','göra,'),
 ('Deut',6,19):('all','alla'),
 ('Deut',6,20):('rätter','rätter,'),
 ('Deut',6,23):(', för att låta oss komma in och giva oss det land',' för att låta oss komma in i och giva oss det land,'),
 ('Deut',6,24):('efter all','efter alla'),
 ('Deut',6,25):('HERREN','HERRENS'),
 # Deuteronomy 7
 ('Deut',7,1):(' di du nu går, för att taga',', dit du nu går, för att taga det'),
 ('Deut',7,4):('eder','eder,'),
 ('Deut',7,5):('dem','dem:'),
 ('Deut',7,6):(' som är helgat åt HERREN, din Gud; dig har HERREN, din Gud, utvalt till att vara hans egendomsfolk,',', som är helgat åt HERREN, din Gud; dig har HERREN, din Gud, utvalt till att vara hans egendomsfolk'),
 ('Deut',7,8):(' som han hade svurit',', som han hade svurit edra'),
 ('Deut',7,11):('rätter','rätter,'),
 ('Deut',7,18):(' gjorde med Farao och med all',', gjorde med Farao och med alla'),
 # Deuteronomy 8
 ('Deut',8,1):(' skolen I hållen, och efter dem skolen I göra, för att I mån komma in i och taga i besittning det land',', skolen I hålla, och efter dem skolen I göra, för att I mån komma in i och taga i besittning det land,'),
 ('Deut',8,2):('åt har låtit dig vandra i öknen, för att tukta dig och pröva dig, så att han kunde förnimma','år har låtit dig vandra i öknen för att tukta dig och pröva dig, så att han kunde förnimma,'),
 ('Deut',8,3):(' som du förut icke visste av, och som icke heller dina fäder visste av; på det att han skulle lära dig förstå',', som du förut icke visste av, och som icke heller dina fäder visste av; på det att han skulle lära dig förstå,'),
 ('Deut',8,5):('hjärta','hjärta,'),
 ('Deut',8,7):('ett land','ett land,'),
 ('Deut',8,9):('ett land d','ett land, d'),
 ('Deut',8,12):('mätt,','mätt'),
 ('Deut',8,13):('guld förökas,','guld förökas'),
 ('Deut',8,15):('vatten ','vatten fanns, och som där lät vatten '),
 ('Deut',8,16):('mat','mat,'),
 ('Deut',8,18):(' att det är HERREN, din Gud, som giver dig kraft att förvärva rikedom, därför att han vill upprätt',', att det är HERREN, din Gud, som giver dig kraft att förvärva rikedom, därför att han vill upprätthåll'),
 # Deuteronomy 9
 ('Deut',9,2):('hört','hört,'),
 ('Deut',9,3):('veta','veta,'),
 ('Deut',9,4):('gör','gör,'),
 ('Deut',9,5):(' att du får komma in i deras land och taga det i besittning, utan dessa hedningars ogudaktighet är det som gör att HERREN, din Gud, fördriver dem för dig. Så vill ock HERREN uppfylla',', att du får komma in i deras land och taga det i besittning, utan dessa hedningars ogudaktighet är det som gör, att HERREN, din Gud, fördriver dem för dig. Så vill ock HERREN uppfylla,'),
 ('Deut',9,6):(' att det icke är din rättfärdighet som gör',', att det icke är din rättfärdighet, som gör,'),
 ('Deut',9,7):('dag','dag,'),
 ('Deut',9,9):('nätter,','nätter'),
 ('Deut',9,10):(', den dag',' den dag,'),
 ('Deut',9,13):('j','J'),
 ('Deut',9,16):(' att I hav',', att I had'),
 ('Deut',9,18):(' i fyrtio dagar och fyrtio nätter, utan att äta och utan att dricka, för all den synds skulle som I haden begått genom att göra',', i fyrtio dagar och fyrtio nätter utan att äta och utan att dricka, för all den synds skull, som I haden begått genom att göra,'),
 ('Deut',9,19):('uppfylld','uppfylld,'),
 ('Deut',9,23):(' som jag har givit eder», då voren I gensträviga mot HERREN',', som jag har givit eder», då voren I gensträviga mot HERRENS'),
 ('Deut',9,24):('dag','dag,'),
 ('Deut',9,25):('sagt','sagt,'),
 ('Deut',9,28):('säga i det land','säga i det land,'),
 # Deuteronomy 10
 ('Deut',10,1):('dig och','dig ock'),
 ('Deut',10,4):('ord','ord,'),
 ('Deut',10,5):('vända','vände'),
 ('Deut',10,6):('som','son'),
 ('Deut',10,7):('trakt','trakt,'),
 ('Deut',10,8):('honom,','honom'),
 ('Deut',10,11):(', och låt dem bryta upp, för att de må komma in i och taga i besittning det land',' och låt dem bryta upp, för att de må komma in i och taga i besittning det land,'),
 ('Deut',10,12):('var','vad'),
 ('Deut',10,14):('tillhör','tillhöra'),
 ('Deut',10,21):('din Gud, som har gjort med dig de stora och underbara gärningar','är din Gud, som har gjort med dig de stora och underbara gärningar,'),
 # Deuteronomy 11
 ('Deut',11,1):('och hålla','och hålla,'),
 ('Deut',11,3):('gärningar','gärningar,'),
 ('Deut',11,7):('gärningar','gärningar,'),
 ('Deut',11,8):(' som jag i dag giver dig, på det att I med frimodighet mån kunna gå in i och intaga det land dit I nu dragen,',', som jag i dag giver dig, på det att I med frimodighet mån kunna gå in i och intaga det land, dit I nu dragen'),
 ('Deut',11,10):(' dit du nu kommer,',', dit du nu kommer'),
 ('Deut',11,11):(' dit I nu dragen,',', dit I nu dragen'),
 ('Deut',11,13):('i nu hören de bud','I nu hören de bud,'),
 ('Deut',11,19):('och när du ','och när du går på vägen, när du lägger dig och när du '),
 ('Deut',11,22):(' som jag giver eder',', som jag giver eder,'),
 # Deuteronomy 12
 ('Deut',12,1):('jorden','jorden,'),
 ('Deut',12,2):(' där de folk som I fördriven',', där de folk, som I fördriven,'),
 ('Deut',12,5):(' som HERREN, eder Gud, utväljer inom någon av edra stammar till att där fästa sitt namn, denna boning skolen I söka',', som HERREN, eder Gud, utväljer inom någon av edra stammar till att där fästa sitt namn, denna boning skolen I söka,'),
 ('Deut',12,8):(' såsom vi nu göra här, var och en vad honom tyckes vara rättast.',', såsom vi nu göra här, var och en vad honom tyckes vara rättast;'),
 ('Deut',12,10):(', d',', s'),
 ('Deut',12,11):('löftesoffer','löftesoffer,'),
 ('Deut',12,12):(', eder Guds, ansikte, med edra söner och döttrar, edra tjänare och tjänarinnor,','S, eder Guds, ansikte, med edra söner och döttrar, edra tjänare och tjänarinnor'),
 ('Deut',12,13):('falla din','falla dig'),
 ('Deut',12,16):('förtära','förtära;'),
 ('Deut',12,17):(' av din säd, ditt vin och din olja, ej heller det förstfödda av dina fäkreatur och din småboskap, ej heller något av de löftesoffer som du lovar, eller av dina frivilliga offer,','n av din säd, ditt vin och din olja, ej heller det förstfödda av dina fäkreatur och din småboskap, ej heller något av de löftesoffer som du lovar, eller av dina frivilliga offer'),
 ('Deut',12,18):(', din Guds, ansikte, på den plats som HERREN, din Gud, utväljer skall du äta sådant, med din son och din dotter, din tjänare och din tjänarinna,','S, din Guds, ansikte, på den plats som HERREN, din Gud, utväljer skall du äta sådant, med din son och din dotter, din tjänare och din tjänarinna'),
 ('Deut',12,21):('namn','namn,'),
 ('Deut',12,22):('skall äta ','skall äta det '),
 ('Deut',12,26):('gåvor','gåvor,'),
 ('Deut',12,28):(' som jag giver dig skall du hålla och höra, för att det må gå dig väl och dina barn efter dig, till evig tid, när du gör var',', som jag giver dig, skall du hålla och höra, för att det må gå dig väl och dina barn efter dig, till evig tid, när du gör vad'),
 ('Deut',12,29):('folk','folk,'),
 ('Deut',12,31):(', och som han hatar, det hava de gjort till sina gudars ära; ja, de gå så långt',' och som han hatar, det hava de gjort till sina gudars ära; ja, de gå så långt,'),
 # Deuteronomy 13
 ('Deut',13,2):('under,','under'),
 ('Deut',13,3):(', för att förnimma',' för att förnimma,'),
 ('Deut',13,5):('väg','väg,'),
 ('Deut',13,6):(', eller hustrun i din famn, eller din vän',' eller hustrun i din famn eller din vän,'),
 ('Deut',13,9):(',',''),
 ('Deut',13,12):(' att man i någon av de städer, som HERREN vill giva dig till att bo i, berättar',', att man i någon av de städer, som HERREN vill giva dig till att bo i, berättar,'),
 ('Deut',13,13):('onda män','onda män,'),
 ('Deut',13,14):('visst','visst,'),
 ('Deut',13,17):('vi','vid'),
 # Deuteronomy 14
 ('Deut',14,2):(' som är helgat åt Herren, din Gud, och dig har Herren',', som är helgat åt HERREN, din Gud, och dig har HERREN'),
 ('Deut',14,4):('fyrfotadjur','fyrfotadjur,'),
 ('Deut',14,7):('eder s','eder sås'),
 ('Deut',14,10):('så ','så'),
 ('Deut',14,16):('uven','uven,'),
 ('Deut',14,21):(' inom dina portar må du giva sådant, och han må äta det; eller ock må du sälja det åt en utlänning. Ty du är ett folk som är helga','r inom dina portar må du giva sådant, och han må äta det; eller ock må du sälja det åt en utlänning. Ty du är ett folk, som är helgat å'),
 ('Deut',14,23):('Herrens, din Guds, ansikte, på den plats som han utväljer till boning åt sitt namn: tionden av din säd, ditt vin och din olja, så ock din förstfödda av dina fäkreatur och din småboskap; ty du skall lära att frukta Herren','HERRENS, din Guds, ansikte, på den plats som han utväljer till boning åt sitt namn: tionden av din säd, ditt vin och din olja, så ock det förstfödda av dina fäkreatur och din småboskap; ty du skall lära att frukta HERREN'),
 ('Deut',14,24):('Herren, din Gud, utväljer till att där fästa sitt namn','HERREN, din Gud, utväljer till att där fästa sitt namn,'),
 ('Deut',14,25):('Herren','HERREN'),
 ('Deut',14,26):(' fäkreatur eller småboskap,',', fäkreatur eller småboskap'),
 ('Deut',14,27):('portar','portar,'),
 ('Deut',14,29):('då','de'),
 # Deuteronomy 15
 ('Deut',15,2):(' som har lånat något åt sin nästa',', som har lånat något åt sin nästa,'),
 ('Deut',15,4):('Herren skall rikligen välsigna dig i det land som HERREN, din Gud, vill giva dig till besittning,','HERREN skall rikligen välsigna dig i det land som HERREN, din Gud, vill giva dig till besittning'),
 ('Deut',15,5):('dig','dig,'),
 ('Deut',15,6):('Herren','HERREN'),
 ('Deut',15,7):('Herren','HERREN'),
 ('Deut',15,9):('Herren','HERREN'),
 ('Deut',15,10):('Herren','HERREN'),
 ('Deut',15,14):('Herren, din Gud, har välsignat dig','HERREN, din Gud, har välsignat dig,'),
 ('Deut',15,15):(' att du själv har varit en träl i Egyptens land, och att Herren',', att du själv har varit en träl i Egyptens land, och att HERREN'),
 ('Deut',15,16):('till dig','till dig,'),
 ('Deut',15,18):(' att du måste släppa din tjänare fri ur din tjänst; i sex år har han ju berett dig dubbelt så stor förmån som någon avlönad legodräng. Så skall Herren',', att du måste släppa din tjänare fri ur din tjänst; i sex år har han ju berett dig dubbelt så stor förmån som någon avlönad legodräng. Så skall HERREN'),
 ('Deut',15,19):('Herren','HERREN'),
 ('Deut',15,21):('Herren','HERREN'),
 # Deuteronomy 16
 ('Deut',16,1):('Herrens, din Guds, påskhögtid; ty i månaden Abib förde Herren','HERRENS, din Guds, påskhögtid; ty i månaden Abib förde HERREN'),
 ('Deut',16,3):('den dag','den dag,'),
 ('Deut',16,4):(',',''),
 ('Deut',16,5):('Herren','HERREN'),
 ('Deut',16,6):('Herren, din Gud, utväljer till boning åt sitt namn, och där skall du slakta påskoffret om aftonen, när solen går ned den tid på dagen, då den','HERREN, din Gud, utväljer till boning åt sitt namn, och där skall du slakta påskoffret om aftonen, när solen går ned, den tid på dagen, då du'),
 ('Deut',16,7):('Herren','HERREN'),
 ('Deut',16,8):('Herrens','HERRENS'),
 ('Deut',16,9):(' då man begynner skära säden',', då man begynner skära säden,'),
 ('Deut',16,10):('Herrens, din Guds, veckohögtid och bära fram din hands frivilliga gåva, som du må giva efter råd och lägenhet, alltefter måttet av den välsignelse som Herren','HERRENS, din Guds, veckohögtid och bära fram din hands frivilliga gåva, som du må giva efter råd och lägenhet, alltefter måttet av den välsignelse som HERREN'),
 ('Deut',16,11):('Herrens, din Guds, ansikte skall du glädja dig på den plats som Herren, din Gud, utväljer till boning åt sitt namn, du själv med din son och din dotter, din tjänare och tjänarinna, och med leviten som bor inom dina portar, och främlingen, den faderlöse och änkan','HERRENS, din Guds, ansikte skall du glädja dig på den plats som HERREN, din Gud, utväljer till boning åt sitt namn, du själv med din son och din dotter, din tjänare och tjänarinna och med leviten som bor inom dina portar, och främlingen, den faderlöse och änkan,'),
 ('Deut',16,12):('ihåg','ihåg,'),
 ('Deut',16,14):('änkan','änkan,'),
 ('Deut',16,15):('Herrens, din Guds, högtid, på den plats som Herren','HERRENS, din Guds, högtid, på den plats som HERREN'),
 ('Deut',16,16):('mankön träda fram inför Herrens','mankön träda fram inför HERRENS'),
 ('Deut',16,17):(': giva vad hans hand förmår, alltefter måttet av den välsignelse som Herren',' giva vad hans hand förmår, alltefter måttet av den välsignelse som HERREN'),
 ('Deut',16,18):('Herren','HERREN'),
 # Deuteronomy 17
 ('Deut',17,1):('Herren, din Gud, något djur av fäkreaturen eller av småboskapen, som har något lyte eller något annat fel, ty sådant är en styggelse för Herren','HERREN, din Gud, något djur av fäkreaturen eller av småboskapen, som har något lyte eller något annat fel, ty sådant är en styggelse för HERREN'),
 ('Deut',17,2):('Herren, din Gud, vill giva dig, någon man eller kvinna befinnes göra vad ont är i Herrens','HERREN, din Gud, vill giva dig, någon man eller kvinna befinnes göra vad ont är i HERRENS'),
 ('Deut',17,4):('visst','visst,'),
 ('Deut',17,5):('gärning','gärning,'),
 ('Deut',17,7):(',',''),
 ('Deut',17,8):(' varom man tvistar i dina portar, så skall du-',', varom man tvistar i dina portar, så skall du '),
 ('Deut',17,9):('dig','dig,'),
 ('Deut',17,10):('Herren utväljer.','HERREN utväljer,'),
 ('Deut',17,12):('Herren','HERREN'),
 ('Deut',17,14):('Herren','HERREN'),
 ('Deut',17,15):('Herren','HERREN'),
 ('Deut',17,16):(', och icke sända sitt folk tillbaka till Egypten för att skaffa de många hästarna, ty Herren',' och icke sända sitt folk tillbaka till Egypten för att skaffa de många hästarna, ty HERREN'),
 # Deuteronomy 18
 ('Deut',18,2):('Herren','HERREN'),
 ('Deut',18,3):('vara','vara,'),
 ('Deut',18,4):('olja,','olja'),
 ('Deut',18,5):('Herren, din Gud, utvalt bland alla dina stammar, för att han och hans söner alltid skola stå och göra tjänst i Herrens','HERREN, din Gud, utvalt bland alla dina stammar, för att han och hans söner alltid skola stå och göra tjänst i HERRENS'),
 ('Deut',18,6):('Herren','HERREN'),
 ('Deut',18,12):('Herren','HERREN'),
 ('Deut',18,14):(' som du nu fördriver',', som du nu fördriver,'),
 ('Deut',18,16):('dag','dag,'),
 ('Deut',18,20):(' som är så förmäten, att han i mitt namn talar',', som är så förmäten, att han i mitt namn talar,'),
 ('Deut',18,22):('namn,','namn'),
 # Deuteronomy 19
 ('Deut',19,3):('landområde','landområde,'),
 ('Deut',19,4):('; om någon dödar sin nästa utan vett och vilja,',': om någon dödar sin nästa utan vett och vilja'),
 ('Deut',19,5):(', och hans hand hugger till med yxan för att fälla trädet,',' och hans hand hugger till med yxan för att fälla trädet'),
 ('Deut',19,7):('det','det,'),
 ('Deut',19,8):(' som han har sagt',', som han har sagt,'),
 ('Deut',19,11):('döds,','döds'),
 ('Deut',19,15):(' att allenast ett vittne träder upp mot någon angående någon missgärning eller synd, vad det nu må vara för en synd',', att allenast ett vittne träder upp mot någon angående någon missgärning eller synd, vad det nu må vara för en synd,'),
 ('Deut',19,21):('öga fo','öga fö'),
 # Deuteronomy 20
 ('Deut',20,1):(' som är större än du.',', som är större än du,'),
 ('Deut',20,5):('icke.','icke,'),
 ('Deut',20,7):('att icke','att icke,'),
 ('Deut',20,11):(' som finnes där',', som finnes där,'),
 ('Deut',20,14):('HERREN.','HERREN,'),
 ('Deut',20,17):('HERREN','HERREN,'),
 ('Deut',20,18):('styggelser','styggelser,'),
 ('Deut',20,19):('människor','människor,'),
 ('Deut',20,20):(' om vilka du vet',', om vilka du vet,'),
 # Deuteronomy 21
 ('Deut',21,1):(', och man icke vet',' och man icke vet,'),
 ('Deut',21,2):(' där den ihjälslagne påträffas',', där den ihjälslagne påträffas,'),
 ('Deut',21,3):('kviga','kviga,'),
 ('Deut',21,6):(' där den ihjälslagne påträffades, skola två sina händer över kvigan',', där den ihjälslagne påträffades, skola två sina händer över kvigan,'),
 ('Deut',21,9):('göra','göra,'),
 ('Deut',21,11):('kvinna','kvinna,'),
 ('Deut',21,14):('gå','gå,'),
 ('Deut',21,15):('förstfödde ','förstfödde är '),
 ('Deut',21,16):('älskar, till förfång för sonen till den han försmår, då nu denne är den förstfödde,','han älskar, till förfång för sonen till den han försmår, då nu denne är den förstfödde;'),
 ('Deut',21,21):('dig','dig,'),
 ('Deut',21,22):('synd','synd,'),
 # Deuteronomy 22
 ('Deut',22,2):('vet','vet,'),
 ('Deut',22,13):('mot henne,','mot henne'),
 ('Deut',22,21):(' därför att hon har gjort vad som var en galenskap i Israel, då hon bedrev otukt i sin faders hus: du skall skaffa bort ifrån dig',', därför att hon har gjort, vad som var en galenskap i Israel, då hon bedrev otukt i sin faders hus: du skall skaffa bort ifrån dig,'),
 ('Deut',22,22):(' som låg hos kvinnan, och jämväl kvinnan: du skall skaffa bort ifrån Israel',', som låg hos kvinnan, och jämväl kvinnan: du skall skaffa bort ifrån Israel,'),
 ('Deut',22,23):('man,','man'),
 ('Deut',22,24):('dig','dig,'),
 ('Deut',22,25):(' som mannen träffade den trolovade flickan, och han där tog henne med våld och lägrade henne, så skall mannen som lägrade henne',', som mannen träffade den trolovade flickan, och han där tog henne med våld och lägrade henne, så skall mannen, som lägrade henne,'),
 ('Deut',22,26):('synd','synd,'),
 ('Deut',22,27):('marken','marken,'),
 ('Deut',22,28):(' som icke är trolovad, och han tager fatt henne och lägrar henne,',', som icke är trolovad, och han tager fatt henne och lägrar henne'),
 ('Deut',22,29):(' som lägrade flickan',', som lägrade flickan,'),
 # Deuteronomy 23
 ('Deut',23,4):('lejde,','lejde'),
 ('Deut',23,8):('led','led,'),
 ('Deut',23,9):('taga,','taga'),
 ('Deut',23,15):('sin herre','sin herre,'),
 ('Deut',23,19):('annat','annat,'),
 ('Deut',23,20):(' dit du nu kommer,',', dit du nu kommer'),
 # Deuteronomy 24
 ('Deut',24,11):(' som du har lånat åt',', som du har lånat åt,'),
 ('Deut',24,13):(' sig',' sig,'),
 ('Deut',24,14):('evad han är en ','evad han är en av '),
 ('Deut',24,16):('barns skull','barns skull,'),
 ('Deut',24,18):('ihåg','ihåg,'),
 # Deuteronomy 25
 ('Deut',25,1):('män,','män'),
 ('Deut',25,2):('ned,','ned'),
 ('Deut',25,7):(', till de äldste, och säga: ',' till de äldste, och säga: »'),
 ('Deut',25,8):(' där han bor',', där han bor,'),
 # Deuteronomy 26
 ('Deut',26,9):('ett land','ett land,'),
 ('Deut',26,11):(' har givit åt dig och ditt hus skall du glädja dig, och jämte dig leviten och främlingen',', har givit åt dig och ditt hus skall du glädja dig och jämte dig leviten och främlingen,'),
 ('Deut',26,12):('änkan,','änkan'),
 ('Deut',26,14):('gjort','gjort,'),
 ('Deut',26,15):('det land','det land,'),
 ('Deut',26,17):('förklara','förklara,'),
 ('Deut',26,18):('förklara','förklara,'),
 # Deuteronomy 27
 ('Deut',27,4):('stenar','stenar,'),
 ('Deut',27,7):('Guds.','Guds,'),
 ('Deut',27,10):('Gud','Guds,'),
 ('Deut',27,12):('Juda.','Juda,'),
 ('Deut',27,15):('man','man,'),
 # Deuteronomy 28
 ('Deut',28,2):('träffa dig','träffa dig,'),
 ('Deut',28,4):('vara,','vara'),
 ('Deut',28,10):('se','se,'),
 ('Deut',28,12):('tid,','tid'),
 ('Deut',28,17):(',',''),
 ('Deut',28,18):('vara,','vara'),
 ('Deut',28,20):('vara','vara,'),
 ('Deut',28,21):(' dit du nu kommer,',', dit du nu kommer'),
 ('Deut',28,36):(' som du sätter över dig bort till ett folk',', som du sätter över dig, bort till ett folk,'),
 ('Deut',28,37):('folk','folk,'),
 ('Deut',28,43):(' som bor hos dig',', som bor hos dig,'),
 ('Deut',28,45):('stadgar','stadgar,'),
 ('Deut',28,46):(',',''),
 ('Deut',28,48):('fiender','fiender,'),
 ('Deut',28,49):(', ett folk',', ett folk,'),
 ('Deut',28,57):('barnen','barnen,'),
 ('Deut',28,62):('HERRENS.','HERRENS,'),
 ('Deut',28,63):('fröjdade sig över eder','fröjdade sig över eder,'),
 # Deuteronomy 29
 ('Deut',29,1):('förbunds','förbunds,'),
 ('Deut',29,2):('Egyptens land,','Egyptens land'),
 ('Deut',29,6):('veta','veta,'),
 ('Deut',29,11):(', och främlingen',' och främlingen,'),
 ('Deut',29,14):('allenast','allenast,'),
 ('Deut',29,16):(' huru vi bodde i Egyptens land, och huru vi drogo mitt igenom de folks land, som I nu haven lämnat',', huru vi bodde i Egyptens land, och huru vi drogo mitt igenom de folks land, som I nu haven lämnat.'),
 ('Deut',29,18):(' vars hjärta i dag vänder sig bort ifrån HERREN, vår Gud, för att gå åstad och tjäna dessa folks gudar; bland eder må icke finnas någon rot',', vars hjärta i dag vänder sig bort ifrån HERREN, vår Gud, för att gå åstad och tjäna dessa folks gudar; bland eder må icke finnas någon rot,'),
 ('Deut',29,19):('tänker','tänker,'),
 ('Deut',29,20):('Herrens vrede och nitälskan skall då vara såsom en rykande eld mot de männen, och all den förbannelse som är uppskriven i denna bok skall komma att vila på honom, och Herren','HERRENS vrede och nitälskan skall då vara såsom en rykande eld mot den mannen, och all den förbannelse, som är uppskriven i denna bok, skall komma att vila på honom, och HERREN'),
 ('Deut',29,22):(' som uppstå efter eder, och främlingen, som kommer ifrån fjärran land, de skola säga, när de se de plågor och sjukdomar',', som uppstå efter eder, och främlingen, som kommer ifrån fjärran land, de skola säga, när de se de plågor och sjukdomar,'),
 ('Deut',29,23):('se','se,'),
 ('Deut',29,24):('Herren','HERREN'),
 ('Deut',29,27):('förbannelse','förbannelse,'),
 # Deuteronomy 30
 ('Deut',30,1):('folk','folk,'),
 ('Deut',30,11):(' som jag i dag giver dig',', som jag i dag giver dig,'),
 ('Deut',30,16):(' dit du nu kommer,',', dit du nu kommer'),
 ('Deut',30,18):(' dit du nu drager över Jordan,',', dit du nu drager över Jordan'),
 ('Deut',30,19):('förbannelse,','förbannelse.'),
 # Deuteronomy 31
 ('Deut',31,4):('dem','dem,'),
 ('Deut',31,5):('med dem','med dem,'),
 ('Deut',31,12):('främlingarna','främlingarna,'),
 ('Deut',31,13):(' dit I nu dragen över Jordan, för att taga det i besittning.',', dit I nu dragen över Jordan, för att taga det i besittning.»'),
 ('Deut',31,14):('närmar sig','närmar sig,'),
 ('Deut',31,16):(' dit de nu komma, och de skola övergiva mig och bryta det förbund',', dit de nu komma, och de skola övergiva mig och bryta det förbund,'),
 ('Deut',31,17):('ibland oss','ibland oss,'),
 ('Deut',31,21):('ju','ju,'),
 ('Deut',31,25):('leviterna','leviterna,'),
 ('Deut',31,29):(' att I efter min död skolen taga eder till, vad fördärvligt är, och vika av ifrån den väg som jag har bjudit eder gå; därför skall olycka träffa eder i kommande dagar, när I gören',', att I efter min död skolen taga eder till, vad fördärvligt är, och vika av ifrån den väg som jag har bjudit eder gå; därför skall olycka träffa eder i kommande dagar, när I gören,'),
 # Deuteronomy 32
 ('Deut',32,20):('se','se,'),
 ('Deut',32,27):(" att deras fiender då skulle vålla mig grämelse, att deras ovänner skulle misstyda det, att de skulle säga: 'Vår hand var så stark, det var icke HERREN",", att deras fiender då skulle vålla mig grämelse, att deras ovänner skulle misstyda det, att de skulle säga: 'Vår hand var så stark, det var icke HERREN,"),
 ('Deut',32,36):('ser','ser,'),
 ('Deut',32,47):(' dit I nu dragen över Jordan,',', dit I nu dragen över Jordan'),
 # Deuteronomy 33
 ('Deut',33,2):('; m','; h'),
 ('Deut',33,4):('fo','fö'),
 ('Deut',33,6):('.','.»'),
 ('Deut',33,17):('tiotusenden.','tiotusenden,'),
 ('Deut',33,19):(', och de skatter',' och de skatter,')}

def chapter_number(name):
    if not name:return 1
    if name[0].isalpha():return 100+10*(ord(name[0])-ord('a'))+int(name[1:]) # Psalms 100-150: 19_a0.html
    return int(name)

def read(path):
    """Return {(code,chapter,verse): text} in 1917 coordinates, from the zip that holds the e-text."""
    source=zipfile.ZipFile(path).read('runeberg-bibeln-1917.html').decode('utf-8')
    lines={}
    pages=re.findall(r'<a name="(\d\d)(?:_(\w\w\w?))?\.html"\s*>.*?<pre>(.*?)</pre>',source,flags=re.S)
    if len(pages)!=1189:raise ValueError('Expected 1189 Runeberg chapters, found %d'%len(pages))
    for book,name,body in pages:
        code=CODES[int(book)-1];chapter=chapter_number(name);state='text';current=None;seen={}
        for line in html.unescape(body).split('\n'):
            line=line.rstrip()
            if not line.strip():
                state='text' if state=='summary' else state;continue
            if state=='notes' or re.match(r'^\[\d+\]',line):
                state='notes';continue
            if TITLE.match(line) or DIVISION.match(line) or line.strip()==SINGLE.get(book):
                state='title';continue
            verse=VERSE.match(line)
            if state in('title','summary') and not verse:
                state='summary';continue
            if line.lstrip().startswith('>') or ELSEWHERE.match(line):continue
            if verse:
                state='text';number=int(verse[1]);seen[number]=seen.get(number,0)+1
                number=RENUMBER.get((book,chapter,number,seen[number]),number)
                if current and number<current[1]:
                    # A restarted count belongs to the following chapter.
                    if number!=1:raise ValueError('Verse order %s %d:%d'%((code,)+current))
                    current=(current[0]+1,number)
                else:current=(current[0] if current else chapter,number)
                key=(code,)+current
                if key in lines:raise ValueError('Duplicate 1917 verse %s %d:%d'%key)
                lines[key]=[verse[2]]
            elif current:lines[(code,)+current].append(line)
            else:
                # The heading interrupted the last verse of the preceding chapter.
                last=max(k for k in lines if k[0]==code and k[1]==chapter-1);lines[last].append(line)
    verses={}
    for key,parts in lines.items():
        text=' '.join(part.strip() for part in parts)
        text=re.sub(r'\[\d+\]|</?spärr>','',text)
        text=re.sub(r'(?:^|\s)----(?=\s|$)',' ',text).replace('--','–')
        text=' '.join(text.split())
        if not text:continue # Verses such as Josh 21:36-37 appear only as a footnote reference.
        if re.search(r'\[\d+\]|[<>]',text):raise ValueError('Unclean 1917 text %s %d:%d'%key)
        verses[key]=text
    for key,(old,new) in REPLACE.items():
        if verses[key].count(old)!=1:raise ValueError('1917 correction does not apply once: %s'%str(key))
        verses[key]=verses[key].replace(old,new)
    return verses
