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
 # Exodus 1-2 (EFS 1927 GT s. 77-79 and EFS 1926 GT s. 69-71): E001-E012, readings both prints agree on.
 ('Exod',1,14):('på marken korteligen, med tvångsarbeten av alla slag, som de läto dem utföra','på marken, korteligen, med tvångsarbeten av alla slag, som de läto dem utföra.'),
 ('Exod',1,17):('gjorde icke såsom','gjorde icke, såsom'),
 ('Exod',1,22):('gossebarn- skolen I kasta i Nilfloden, men all flickebarn','gossebarn skolen I kasta i Nilfloden, men alla flickebarn'),
 ('Exod',2,2):('Och hon såg att det','Och hon såg, att det'),
 ('Exod',2,4):('därifrån, för att se huru det','därifrån för att se, huru det'),
 ('Exod',2,6):('och såg att det var','och såg, att det var'),
 ('Exod',2,7):('Vill du att jag skall gå och kalla hit till dig en hebreisk amma som kan','Vill du, att jag skall gå och kalla hit till dig en hebreisk amma, som kan'),
 ('Exod',2,9):('med dig, och amma','med dig och amma'),
 ('Exod',2,10):('hade vuxit upp; förde hon','hade vuxit upp, förde hon'),
 ('Exod',2,11):('hände sig att Mose, sedan han hade blivit stor, gick ut till sina bröder och såg på deras trälarbete. Och han fick se att en egyptisk man slog en hebreisk man, en av hans bröder.','hände sig, att Mose, sedan han hade blivit stor, gick ut till sina bröder och såg på deras trälarbete. Och han fick se, att en egyptisk man slog en hebreisk man, en av hans bröder;'),
 ('Exod',2,12):('och när han såg att ingen','och när han såg, att ingen'),
 ('Exod',2,20):('med oss».','med oss.»'),
 # Exodus 3 (EFS 1927 GT s. 79-81 and EFS 1926 GT s. 71-72): E013-E026, readings both prints agree on.
 ('Exod',3,2):('buske. Han såg att busken brann','buske. Han såg, att busken brann'),
 ('Exod',3,3):('synen och se varför busken','synen och se, varför busken'),
 ('Exod',3,4):('då HERREN såg att han gick','då HERREN såg, att han gick'),
 ('Exod',3,5):('fötter, ty platsen där du står är helig mark.»','fötter, ty platsen, där du står, är helig mark.»'),
 ('Exod',3,7):('jag har hört huru de ropa över sina plågare; jag vet vad de måste','jag har hört, huru de ropa över sina plågare; jag vet, vad de måste'),
 ('Exod',3,8):('land, ett land som flyter av mjölk och honung, det land där kananéer,','land, ett land, som flyter av mjölk och honung, det land, där kananéer,'),
 ('Exod',3,9):('kommit till mig, och jag därjämte har sett huru egyptierna','kommit till mig och jag därjämte har sett, huru egyptierna'),
 ('Exod',3,13):('och de fråga mig; ','och de fråga mig: '),
 ('Exod',3,14):('har sänt mig till eder.','har sänt mig till eder.»'),
 ('Exod',3,10):('ut ur Egypten»','ut ur Egypten.»'),
 ('Exod',3,16):("äldste i Israel, och säg till dem: HERREN, edra fäders Gud, Abrahams, Isaks och Jakobs Gud, har uppenbarat sig för mig, och han har sagt: 'Jag har sett till eder och har sett det som vederfares eder Egypten;","äldste i Israel och säg till dem: HERREN, edra fäders Gud, Abrahams, Isaks och Jakobs Gud, har uppenbarat sig för mig, och han har sagt: 'Jag har sett till eder och har sett det som vederfares eder i Egypten;"),
 ('Exod',3,17):('land, ett land som flyter av','land, ett land, som flyter av'),
 ('Exod',3,18):("honom: HERREN, hebréernas Gud, har visat sig för oss, så låt oss nu","honom: 'HERREN, hebréernas Gud, har visat sig för oss; så låt oss nu"),
 ('Exod',3,19):('Dock vet jag att konungen','Dock vet jag, att konungen'),
 # Exodus 4 (EFS 1927 GT s. 81-83 and EFS 1926 GT s. 72–74): E027-E037, readings both prints agree on.
 ('Exod',4,5):('»Så skola de. tro att HERREN, deras fäders Gud, Abrahams Gud, Isaks Gud och Jakobs Gud, har uppenbarat sig för dig.','»Så skola de tro, att HERREN, deras fäders Gud, Abrahams Gud, Isaks Gud och Jakobs Gud, har uppenbarat sig för dig.»'),
 ('Exod',4,7):('Åter sade han. »Stick din hand','Åter sade han: »Stick din hand'),
 ('Exod',4,10):('HERREN: »Ack I Herre, jag är','HERREN: »Ack Herre, jag är'),
 ('Exod',4,12):('och lära dig vad du skall','och lära dig, vad du skall'),
 ('Exod',4,14):('leviten? Jag vet att han är en man som kan tala.','leviten? Jag vet, att han är en man, som kan tala.'),
 ('Exod',4,15):('skall lära eder vad I skolen','skall lära eder, vad I skolen'),
 ('Exod',4,18):('bröder i Egypten, för att se om de ännu leva.»','bröder i Egypten för att se, om de ännu leva.»'),
 ('Exod',4,21):('till Egypten så se till, att du inför Farao gör alla de under som jag har','till Egypten, så se till, att du inför Farao gör alla de under, som jag har'),
 ('Exod',4,24):('resan hände sig att HERREN på','resan hände sig, att HERREN på'),
 ('Exod',4,28):('alla de tecken som han hade','alla de tecken, som han hade'),
 ('Exod',4,31):('när de hörde att HERREN hade','när de hörde, att HERREN hade'),
 # Exodus 5 (EFS 1927 GT s. 82-83 and EFS 1926 GT s. 73–74): E038-E043, readings both prints agree on.
 ('Exod',5,4):('edra dagsverken.','edra dagsverken.»'),
 ('Exod',5,8):('samma antal tegel som de förut hava gjort skolen I ändå','samma antal tegel, som de förut hava gjort, skolen I ändå'),
 ('Exod',5,17):('ären lata, ja lata ären I.','ären lata, ja, lata ären I.'),
 ('Exod',5,19):('tillsyningsmän att det var illa ställt för dem, eftersom de hade fått det svaret att de icke','tillsyningsmän, att det var illa ställt för dem, eftersom de hade fått det svaret, att de icke'),
 ('Exod',5,21):('att dräpa oss.','att dräpa oss.»'),
 ('Exod',5,23):('ifrån den tid då jag gick till Farao för att tala i ditt namn har han ju gjort illa mot detta folk, och du har ingalunda räddat ditt folk.','ifrån den tid, då jag gick till Farao för att tala i ditt namn, har han ju gjort illa mot detta folk, och du har ingalunda räddat ditt folk.»'),
 # Exodus 6 (EFS 1927 GT s. 83-85 and EFS 1926 GT s. 74–75): E044-E049, readings both prints agree on.
 ('Exod',6,1):('skall du få se vad jag skall','skall du få se, vad jag skall'),
 ('Exod',6,4):('land, det land där de bodde','land, det land, där de bodde'),
 ('Exod',6,7):('skolen förnimma att jag är HERREN eder Gud, han','skolen förnimma, att jag är HERREN, eder Gud, han'),
 ('Exod',6,8):("Jag är HERREN.'»",'Jag är HERREN.»'),
 ('Exod',6,14):('deras familjer. Rubens, Israels','deras familjer: Rubens, Israels'),
 ('Exod',6,27):('Mose och Aron,','Mose och Aron.'),
 # Exodus 7 (EFS 1927 GT s. 85-86 and EFS 1926 GT s. 76-77): E050-E056, readings both prints agree on.
 ('Exod',7,5):('skola förnimma att jag är','skola förnimma, att jag är'),
 ('Exod',7,6):('de gjorde såsom HERREN','de gjorde, såsom HERREN'),
 ('Exod',7,10):('Farao och gjorde såsom HERREN','Farao och gjorde, såsom HERREN'),
 ('Exod',7,15):('din hand staven som','din hand staven, som'),
 ('Exod',7,17):('skall du förnimma att jag','skall du förnimma, att jag'),
 ('Exod',7,19):('din stav, och räck','din stav och räck'),
 ('Exod',7,20):('Aron gjorde såsom HERREN hade bjudit. Han lyfte upp staven och slog vattnet i Nilfloden inför Faraos och hans tjänares ögon; då förvandlades allt vatten floden till blod.','Aron gjorde, såsom HERREN hade bjudit. Han lyfte upp staven och slog vattnet i Nilfloden inför Faraos och hans tjänares ögon; då förvandlades allt vatten i floden till blod.'),
 # Exodus 8 (EFS 1927 GT s. 86-88 and EFS 1926 GT s. 77-79): E057-E067, readings both prints agree on.
 ('Exod',8,3):('i din säng, och in i dina tjänares hus och bland ditt folk, och i dina bakugnar','i din säng och in i dina tjänares hus och bland ditt folk och i dina bakugnar'),
 ('Exod',8,9):('en tid inom vilken jag, genom att bedja för dig och dina tjänare och ditt folk, skall skaffa bort paddorna från dig och dina hus, så att de finnas kvar allenast i Nilfloden.','en tid, inom vilken jag genom att bedja för dig och dina tjänare och ditt folk skall skaffa bort paddorna från dig och dina hus, så att de finnas kvar allenast i Nilfloden.»'),
 ('Exod',8,10):('Må det ske såsom du har sagt, så att du får förnimma att ingen','Må det ske, såsom du har sagt, så att du får förnimma, att ingen'),
 ('Exod',8,13):('Och HERREN gjorde såsom Mose hade begärt','Och HERREN gjorde, såsom Mose hade begärt'),
 ('Exod',8,15):('när Farao såg att han','när Farao såg, att han'),
 ('Exod',8,20):('honom: så säger HERREN: Släpp mitt folk, så att de kunna hålla guds tjänst åt mig.','honom: Så säger HERREN: Släpp mitt folk, så att de kunna hålla gudstjänst åt mig.'),
 ('Exod',8,21):('icke släpper mitt folk, de, då skall jag sända svärmar av flugor över dig och dina tjänare och ditt folk och dina hus, så att egyptiernas hus skola bliva uppfyllda av flugsvärmar, ja, själva marken på vilken','icke släpper mitt folk, se, då skall jag sända svärmar av flugor över dig och dina tjänare och ditt folk och dina hus, så att egyptiernas hus skola bliva uppfyllda av flugsvärmar, ja, själva marken, på vilken'),
 ('Exod',8,22):('må förnimma att jag','må förnimma, att jag'),
 ('Exod',8,26):('icke an att vi','icke an, att vi'),
 ('Exod',8,28):('Bedjen för mig.','Bedjen för mig.»'),
 ('Exod',8,31):('Och HERREN gjorde såsom Mose sade begärt','Och HERREN gjorde, såsom Mose hade begärt'),
 # Exodus 9 (EFS 1927 GT s. 88-89 and EFS 1926 GT s. 79-80): E068-E077, readings both prints agree on.
 ('Exod',9,7):('han släppte icke folket','han släppte icke folket.'),
 ('Exod',9,14):('må förnimma att ingen','må förnimma, att ingen'),
 ('Exod',9,15):('min land','min hand'),
 ('Exod',9,19):('all boskap som då finnas','all boskap, som då finnas'),
 ('Exod',9,23):('jorden, så lät HERREN','jorden; så lät HERREN'),
 ('Exod',9,27):('Det är HERREN som är','Det är HERREN, som är'),
 ('Exod',9,29):('må förnimma att landet','må förnimma, att landet'),
 ('Exod',9,30):('Dock vet jag väl att du','Dock vet jag väl, att du'),
 ('Exod',9,31):('hade gått i ax och linet','hade gått i ax, och linet'),
 ('Exod',9,34):('Farao såg att regnet','Farao såg, att regnet'),
 # Exodus 10 (EFS 1927 GT s. 89-91 and EFS 1926 GT s. 80-82): E078-E083, readings both prints agree on.
 ('Exod',10,2):('din sonson vilka stora gärningar jag har utfört bland egyptierna, och vilka tecken jag har gjort bland dem, så att I förnimmen att jag','din sonson, vilka stora gärningar jag har utfört bland egyptierna, och vilka tecken jag har gjort bland dem, så att I förnimmen, att jag'),
 ('Exod',10,5):('övertäcka marken så','övertäcka marken, så'),
 ('Exod',10,6):('dina faders fäder','dina fäders fäder'),
 ('Exod',10,7):('icke ännu att','icke ännu, att'),
 ('Exod',10,10):('Må HERREN: vara med eder lika visst som jag släpper eder med edra kvinnor och barn! Där ser man att I','Må HERREN vara med eder lika visst som jag släpper eder med edra kvinnor och barn! Där ser man, att I'),
 ('Exod',10,23):('hade ljust där','hade ljust, där'),
 # Exodus 11 (EFS 1927 GT s. 91-92 and EFS 1926 GT s. 82-82): E084-E085, readings both prints agree on.
 ('Exod',11,2):('folket, och säg att var','folket och säg, att var'),
 ('Exod',11,7):('förnimma att HERREN','förnimma, att HERREN'),
 # Exodus 12 (EFS 1927 GT s. 92-94 and EFS 1926 GT s. 82-84): E086-E098, readings both prints agree on.
 ('Exod',12,7):('i husen där','i husen, där'),
 ('Exod',12,11):('HERRENS Påsk.','HERRENS påsk.'),
 ('Exod',12,13):('på de hus i vilka','på de hus, i vilka'),
 ('Exod',12,19):('var och en son äter','var och en som äter'),
 ('Exod',12,20):('ären bosatta skolen','ären bosatta, skolen'),
 ('Exod',12,21):('Begiven eder hem, och','Begiven eder hem och'),
 ('Exod',12,22):('det övre dörr träet och båda dörrposterna med blodet som är i skålen; och ingen av eder må gå ut genom sin hus dörr','det övre dörrträet och båda dörrposterna med blodet som är i skålen; och ingen av eder må gå ut genom sin husdörr'),
 ('Exod',12,23):('när ha ser blodet på det övre dörrträet och på de två dörrposterna, skall HERREN gå förbi dörren och icke tillstädja Fördärvaren att komma i i edra','när han ser blodet på det övre dörrträet och på de två dörrposterna, skall HERREN gå förbi dörren och icke tillstädja Fördärvaren att komma in i edra'),
 ('Exod',12,28):('de gjorde såsom','de gjorde, såsom'),
 ('Exod',12,35):('hade gjort såsom Mose','hade gjort, såsom Mose'),
 ('Exod',12,36):('de gåvo dem vad','de gåvo dem, vad'),
 ('Exod',12,41):('på den dag då de fyra hundra trettio åren voro förlidna drogo','på den dag, då de fyra hundra trettio åren voro förlidna, drogo'),
 ('Exod',12,44):('men en träl som är köpt för penningar må äta därav, sedan du ha omskurit','men en träl, som är köpt för penningar, må äta därav, sedan du har omskurit'),
 # Exodus 13 (EFS 1927 GT s. 94-95 and EFS 1926 GT s. 85-86): E099-E103, readings both prints agree on.
 ('Exod',13,5):('ett land som flyter','ett land, som flyter'),
 ('Exod',13,7):('hos dig, i hela','hos dig i hela'),
 ('Exod',13,13):('bland åsnor som öppnar','bland åsnor, som öppnar'),
 ('Exod',13,17):('Gud tänkte att folket','Gud tänkte, att folket'),
 ('Exod',13,21):('i en molnstod, för att leda dem på vägen, och om natten i en eldstod, för att lysa','i en molnstod för att leda dem på vägen, och om natten i en eldstod för att lysa'),
 # Exodus 14 (EFS 1927 GT s. 96-97 and EFS 1926 GT s. 86-87): E104-E116, readings both prints agree on.
 ('Exod',14,2):('Israels barn att de skola','Israels barn, att de skola'),
 ('Exod',14,3):('tänka att Israels','tänka, att Israels'),
 ('Exod',14,4):('må förnimma att jag','må förnimma, att jag'),
 ('Exod',14,5):('i Egypten att folket','i Egypten, att folket'),
 ('Exod',14,7):('utvalda vagnar, och alla vagnar som eljest','utvalda vagnar och alla vagnar, som eljest'),
 ('Exod',14,10):('fingo se att egyptierna','fingo se, att egyptierna'),
 ('Exod',14,13):('skolen I se vilken','skolen I se, vilken'),
 ('Exod',14,15):('Israels barn att de draga','Israels barn, att de draga'),
 ('Exod',14,16):('din stav, och räck ut din hand över havet, och klyv','din stav och räck ut din hand över havet och klyv'),
 ('Exod',14,18):('skola förnimma att jag','skola förnimma, att jag'),
 ('Exod',14,27):('och egyptierna som flydde möttes','och egyptierna, som flydde, möttes'),
 ('Exod',14,28):('Och vattnet som vände tillbaka övertäckte','Och vattnet, som vände tillbaka, övertäckte'),
 ('Exod',14,31):('när Israel såg huru','när Israel såg, huru'),
 # Exodus 15 (EFS 1927 GT s. 97-99 and EFS 1926 GT s. 87-89): E117-E120, readings both prints agree on.
 ('Exod',15,2):('min lovsång, Och han','min lovsång, och han'),
 ('Exod',15,8):('samlades hög','samlades i hög'),
 ('Exod',15,16):('o HERRE medan','o HERRE, medan'),
 ('Exod',15,26):('och gör vad rätt','och gör, vad rätt'),
 # Exodus 16 (EFS 1927 GT s. 99-100 and EFS 1926 GT s. 89-90): E121-E135, readings both prints agree on.
 ('Exod',16,4):('på prov, för att se om de vilja','på prov för att se, om de vilja'),
 ('Exod',16,5):('tillreda vad de hava','tillreda, vad de hava'),
 ('Exod',16,6):('förnimma att det är HERREN som har','förnimma, att det är HERREN, som har'),
 ('Exod',16,7):('härlighet, HERREN har nämligen hört huru','härlighet; HERREN har nämligen hört, huru'),
 ('Exod',16,8):('giver eder kött att äta, och i morgon bröd att mätta eder med då nu HERREN har hört huru','giver eder kött att äta och i morgon bröd att mätta eder med, då nu HERREN har hört, huru'),
 ('Exod',16,9):('menighet Träden fram inför HERREN, ty han har hört huru','menighet: Träden fram inför HERREN, ty han har hört, huru'),
 ('Exod',16,12):('Jag har hört huru Israels barn knorra. Tala till dem och säg: Vid aftontiden skolen I få kött att äta och i morgon skolen I få bröd att mätta eder med; så skolen I förnimma att jag','Jag har hört, huru Israels barn knorra. Tala till dem och säg: Vid aftontiden skolen I få kött att äta, och i morgon skolen I få bröd att mätta eder med; så skolen I förnimma, att jag'),
 ('Exod',16,14):('när daggen som hade fallit gick bort','när daggen, som hade fallit, gick bort'),
 ('Exod',16,15):('Ty de visste icke vad det var. Men Mose sade till dem: »Detta är brödet som HERREN','Ty de visste icke, vad det var. Men Mose sade till dem: »Detta är brödet, som HERREN'),
 ('Exod',16,18):('och de som hade samlat litet','och den som hade samlat litet'),
 ('Exod',16,21):('solhettan kom smälte','solhettan kom, smälte'),
 ('Exod',16,23):('Baken nu vad I viljen baka, och koken vad I viljen koka','Baken nu, vad I viljen baka, och koken, vad I viljen koka'),
 ('Exod',16,32):('det bröd som jag gav','det bröd, som jag gav'),
 ('Exod',16,33):('en gomer manna, och ställ','en gomer manna och ställ'),
 ('Exod',16,34):('Då gjorde man såsom','Då gjorde man, såsom'),
 # Exodus 17 (EFS 1927 GT s. 101-101 and EFS 1926 GT s. 91-91): E136-E139, readings both prints agree on.
 ('Exod',17,5):('folket, och tag med dig några av de äldste i Israel. Och tag i din hand staven med vilken','folket och tag med dig några av de äldste i Israel. Och tag i din hand staven, med vilken'),
 ('Exod',17,9):('åt oss, och drag så åstad till strid mot Amalek. I morgon skall jag ställa mig överst på höjden, med Guds','åt oss och drag så åstad till strid mot Amalek. I morgon skall jag ställa mig överst på höjden med Guds'),
 ('Exod',17,10):('Josua gjorde såsom','Josua gjorde, såsom'),
 ('Exod',17,16):('betygar att HERREN','betygar, att HERREN'),
 # Exodus 18 (EFS 1927 GT s. 101-103 and EFS 1926 GT s. 91-92): E140-E144, readings both prints agree on.
 ('Exod',18,8):('de vedermödor som de hade','de vedermödor, som de hade'),
 ('Exod',18,11):('Nu vet jag att HERREN','Nu vet jag, att HERREN'),
 ('Exod',18,14):('ensam under det att','ensam, under det att'),
 ('Exod',18,21):('dugande män som frukta Gud, pålitliga män som hata','dugande män, som frukta Gud, pålitliga män, som hata'),
 ('Exod',18,25):('föreståndare somliga','föreståndare, somliga'),
 # Exodus 19 (EFS 1927 GT s. 103-104 and EFS 1926 GT s. 93-94): E145-E150, readings both prints agree on.
 ('Exod',19,1):('På den dag då den tredje månaden ingick efter Israels barns uttåg ur Egyptens land kommo','På den dag, då den tredje månaden ingick efter Israels barns uttåg ur Egyptens land, kommo'),
 ('Exod',19,4):('själva sett vad jag','själva sett, vad jag'),
 ('Exod',19,6):('Detta är vad du','Detta är, vad du'),
 ('Exod',19,10):('Gå till folket, och helga dem i dag och i morgon, och I åt dem','Gå till folket och helga dem i dag och i morgon, och låt dem'),
 ('Exod',19,23):('och sagt att jag','och sagt, att jag'),
 ('Exod',19,24):('Gå ditned, och kom sedan åter upp och hav Aron med dig. Men prästerna och folket må icke tränga sig fram för att stiga upp till HERREN på det att','Gå ditned och kom sedan åter upp och hav Aron med dig. Men prästerna och folket må icke tränga sig fram för att stiga upp till HERREN, på det att'),
 # Exodus 20 (EFS 1927 GT s. 104-105 and EFS 1926 GT s. 94-95): E151-E153, readings both prints agree on.
 ('Exod',20,10):('främlingen som är hos dig','främlingen, som är hos dig'),
 ('Exod',20,22):('förnummit att jag','förnummit, att jag'),
 ('Exod',20,24):('på den plats där jag stiftar en åminnelse åt mitt namn skall jag','på den plats, där jag stiftar en åminnelse åt mitt namn, skall jag'),
 # Exodus 21 (EFS 1927 GT s. 105-107 and EFS 1926 GT s. 95-96): E154-E166, readings both prints agree on.
 ('Exod',21,1):('de rätter som du','de rätter, som du'),
 ('Exod',21,2):('givas fri, utan lösen.','givas fri utan lösen.'),
 ('Exod',21,7):('icke givas fri såsom','icke givas fri, såsom'),
 ('Exod',21,11):('givas fri, utan lösen och','givas fri utan lösen och'),
 ('Exod',21,13):('en ort dit han','en ort, dit han'),
 ('Exod',21,14):('begår det dådet att','begår det dådet, att'),
 ('Exod',21,18):('med varandra, och den ene','med varandra och den ene'),
 ('Exod',21,22):('med varandra, och någon av dem stöter till en havande kvinna, så att hon föder fram sitt foster, men eljest ingen olycka sker, så böte han vad kvinnans man ålägger honom och betale','med varandra och någon av dem stöter till en havande kvinna, så att hon föder fram sitt foster, men eljest ingen olycka sker, så böte han, vad kvinnans man ålägger honom, och betale'),
 ('Exod',21,28):('ägare vara fri','ägare vare fri'),
 ('Exod',21,29):('stångas, och hans ägare','stångas och hans ägare'),
 ('Exod',21,31):('en flicka som har','en flicka, som har'),
 ('Exod',21,33):('täcker över den, och sedan','täcker över den och sedan'),
 ('Exod',21,36):('däremot känt att','däremot känt, att'),
 # Exodus 22 (EFS 1927 GT s. 107-109 and EFS 1926 GT s. 97-98): E167-E178, readings both prints agree on.
 ('Exod',22,3):('när de skedde, då är det blodskuld. Tjuven skall giva full ersättning; äger han intet, så skall han säljas, till gäldande','när det skedde, då är det blodskuld. Tjuven skall giva full ersättning; äger han intet, så skall han säljas till gäldande'),
 ('Exod',22,5):('eller vingård, eller släpper','eller vingård eller släpper'),
 ('Exod',22,6):('fattar i törnhäckar, och därvid','fattar i törnhäckar och därvid'),
 ('Exod',22,7):('att förvara, och detta','att förvara och detta'),
 ('Exod',22,8):('utrönas om han','utrönas, om han'),
 ('Exod',22,9):('någon påstår att en','någon påstår, att en'),
 ('Exod',22,11):('Så skall det dem emellan komma till en ed vid HERREN, för att det må utrönas om den ene icke har förgripit sig på den andres tillhörighet; denna ed skall ägaren antaga och den andre','så skall det dem emellan komma till en ed vid HERREN, för att det må utrönas, om den ene icke har förgripit sig på den andres tillhörighet; denna ed skall ägaren antaga, och den andre'),
 ('Exod',22,14):('av en annan, och detta bliver skadat eller dör, och dess ägare','av en annan och detta bliver skadat eller dör och dess ägare'),
 ('Exod',22,16):('en jungfru som icke är trolovad och lägrar henne','en jungfru, som icke är trolovad, och lägrar henne'),
 ('Exod',22,17):('penningsumma som man','penningsumma, som man'),
 ('Exod',22,24):('med svärd; så att','med svärd, så att'),
 ('Exod',22,31):('ett heligt: folk','ett heligt folk'),
 # Exodus 23 (EFS 1927 GT s. 109-110 and EFS 1926 GT s. 98-99): E179-E186, readings both prints agree on.
 ('Exod',23,2):('i vad ont är, eller vittna','i vad ont är eller vittna'),
 ('Exod',23,4):('eller åsna som har','eller åsna, som har'),
 ('Exod',23,9):('I veten ju huru','I veten ju, huru'),
 ('Exod',23,12):('må hava ro, och din','må hava ro och din'),
 ('Exod',23,13):('de stycken om vilka jag har talat till eder skolen','de stycken, om vilka jag har talat till eder, skolen'),
 ('Exod',23,21):('mot honom, han skall','mot honom; han skall'),
 ('Exod',23,24):('eller göra såsom','eller göra, såsom'),
 ('Exod',23,26):('någon kvinna som föder','någon kvinna, som föder'),
 # Exodus 24 (EFS 1927 GT s. 110-111 and EFS 1926 GT s. 99-100): E187-E190, readings both prints agree on.
 ('Exod',24,7):('förbundsboken och, föreläste den för folket. Och de sade: »Allt vad HERREN har sagt vilja vi göra och lyda.','förbundsboken och föreläste den för folket. Och de sade: »Allt vad HERREN har sagt vilja vi göra och lyda.»'),
 ('Exod',24,8):('det förbunds som HERREN','det förbunds, som HERREN'),
 ('Exod',24,12):('och budorden som jag har','och budorden, som jag har'),
 ('Exod',24,17):('en förtärande eld, på toppen','en förtärande eld på toppen'),
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
 ('Hag',2,16):('till HERRENS tempel','till HERRENS tempel;')}

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
