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
 # Numbers 1
 ('Num',1,20):('mankön','mankön,'),
 ('Num',1,21):('stam','stam,'),
 ('Num',1,22):('mankön','mankön,'),
 ('Num',1,23):('stam','stam,'),
 ('Num',1,25):('stam','stam,'),
 ('Num',1,27):('stam','stam,'),
 ('Num',1,29):(' son',', som'),
 ('Num',1,33):(';',','),
 ('Num',1,39):('stam','stam,'),
 ('Num',1,45):('barn','barn,'),
 ('Num',1,52):('läger,','läger'),
 ('Num',1,53):('skola iakttaga','skola iakttaga,'),
 ('Num',1,54):('stycken','stycken,'),
 # Numbers 2
 ('Num',2,3):(' son',' son,'),
 ('Num',2,9):('inmönstrade','inmönstrade,'),
 ('Num',2,14):('son','son,'),
 ('Num',2,16):(' som tillhöra Rubens läger',', som tillhöra Rubens läger,'),
 ('Num',2,24):(' som tillhöra Efraims läger',', som tillhöra Efraims läger,'),
 ('Num',2,28):('hans ha','hans hä'),
 ('Num',2,31):(' som tillhöra Dans läger',', som tillhöra Dans läger,'),
 ('Num',2,32):('barn','barn,'),
 ('Num',2,33):(':',''),
 # Numbers 3
 ('Num',3,1):('Detta är berättelsen om Arons och Moses släkt, vid den tid','etta är berättelsen om Arons och Moses släkt vid den tid,'),
 ('Num',3,7):('skola iakttaga','skola iakttaga,'),
 ('Num',3,8):(', och iakttaga',' och iakttaga,'),
 ('Num',3,10):('iakttaga','iakttaga,'),
 ('Num',3,13):(' då jag slog allt förstfött i Egyptens land',', då jag slog allt förstfött i Egyptens land,'),
 ('Num',3,15):(' som äro en månad gamla eller därutöver',', som äro en månad gamla eller därutöver,'),
 ('Num',3,16):('oc','Oc'),
 ('Num',3,22):(' som inmönstrades, i det att man räknade alla dem av mankön som voro en månad gamla eller därutöver',', som inmönstrades, i det att man räknade alla dem av mankön, som voro en månad gamla eller därutöver,'),
 ('Num',3,28):('mankön','mankön,'),
 ('Num',3,30):(';',''),
 ('Num',3,34):(' som inmönstrades, i det att man räknade alla dem av mankön',', som inmönstrades, i det att man räknade alla dem av mankön,'),
 ('Num',3,38):('skulle iakttaga','skulle iakttaga,'),
 ('Num',3,39):(' som Mose och Aron inmönstrade efter deras släkter, enligt HERRENS befallning, alla av mankön',', som Mose och Aron inmönstrade efter deras släkter, enligt HERRENS befallning, alla av mankön,'),
 # Numbers 4
 ('Num',4,3):('män','män,'),
 ('Num',4,5):(' som hänger framför arken',', som hänger framför arken,'),
 ('Num',4,9):('oljekärl','oljekärl,'),
 ('Num',4,11):(',',';'),
 ('Num',4,12):('tahasskinn,','tahasskinn'),
 ('Num',4,14):('därpå alla tillbehör','därpå alla tillbehör,'),
 ('Num',4,15):(' är',' är,'),
 ('Num',4,16):('sons','sons,'),
 ('Num',4,19):(' vad han har att göra eller bära',', vad han har att göra eller bära;'),
 ('Num',4,23):('män','män,'),
 ('Num',4,25):(' av vilka tabernaklet bildas, uppenbarelsetältets täckelse, dess överdrag och det överdrag av tahasskinn',', av vilka tabernaklet bildas, uppenbarelsetältets täckelse, dess överdrag och det överdrag av tahasskinn,'),
 ('Num',4,28):(' som Gersons barns släkter skola hava vid uppenbarelsetältet; och vad de hava att iakttaga skola de utföra under ledning av Itamar, prästen Arons son',', som Gersons barns släkter skola hava vid uppenbarelsetältet; och vad de hava att iakttaga skola de utföra under ledning av Itamar, prästen Arons son.'),
 ('Num',4,30):('män','män,'),
 ('Num',4,31):(' är',' är,'),
 ('Num',4,35):(' alla tjänstbara män',', alla tjänstbara män,'),
 ('Num',4,36):('dem','dem,'),
 ('Num',4,38):('barn','barn,'),
 ('Num',4,39):('män','män,'),
 ('Num',4,41):('släkter','släkter,'),
 ('Num',4,42):('barns släkter','barns släkter,'),
 ('Num',4,43):('män','män,'),
 ('Num',4,45):('släkter','släkter,'),
 ('Num',4,46):('leviterna','leviterna,'),
 ('Num',4,49):('åliggande','åliggande,'),
 # Numbers 5
 ('Num',5,2):('barn','barn,'),
 ('Num',5,9):('gåvor','gåvor,'),
 ('Num',5,27):('exempel','exempel,'),
 # Numbers 6
 ('Num',6,5):('låte','låta'),
 ('Num',6,7):('dö','dö,'),
 ('Num',6,9):('närhet,','närhet'),
 ('Num',6,15):('spis ','spis'),
 ('Num',6,19):(', och därjämte ur korgen en osyrad kaka och en osyrad tunnkaka,',' och därjämte ur korgen en osyrad kaka och en osyrad tunnkaka'),
 # Numbers 7
 ('Num',7,1):(', med alla dess tillbehör, och hade satt upp altaret med alla dess tillbehör,',' med alla dess tillbehör och hade satt upp altaret med alla dess tillbehör'),
 ('Num',7,12):('stam','stam.'),
 ('Num',7,16):(',',''),
 ('Num',7,22):(',',''),
 ('Num',7,25):('spisoffer','spisoffer,'),
 ('Num',7,28):(',',''),
 ('Num',7,34):(',',''),
 ('Num',7,40):(',',''),
 ('Num',7,46):(',',''),
 ('Num',7,52):(',',''),
 ('Num',7,57):('ungtjur','ungtjur,'),
 ('Num',7,58):(',',''),
 ('Num',7,59):('Gamliels','Gamliels,'),
 ('Num',7,64):(',',''),
 ('Num',7,70):(',',''),
 ('Num',7,76):(',',''),
 ('Num',7,82):(',',''),
 # Numbers 8
 ('Num',8,9):('menighet','menighet.'),
 ('Num',8,12):('HERREN,','HERREN'),
 ('Num',8,17):(', Israels barn, både människor och boskap; på den dag då jag slog allt förstfött i Egyptens land',' Israels barn, både människor och boskap; på den dag, då jag slog allt förstfött i Egyptens land,'),
 ('Num',8,19):('söner,','söner'),
 ('Num',8,20):('stycken','stycken,'),
 ('Num',8,23):('och H','Och H'),
 ('Num',8,24):('Detta är','Detta är,'),
 # Numbers 9
 ('Num',9,1):('Sinais öken,','i Sinais öken'),
 ('Num',9,2):('påsk ','påsk'),
 ('Num',9,4):('barn','barn,'),
 ('Num',9,5):(' månaden, på fjortonde dagen i månaden, vid aftontiden, i Sinais öken Israels barn gjorde i alla stycken såsom HERREN hade bjudit Mose','a månaden, på fjortonde dagen i månaden, vid aftontiden, i Sinais öken. Israels barn gjorde i alla stycken, såsom HERREN hade bjudit Mose.'),
 ('Num',9,6):('några män','några män,'),
 ('Num',9,8):('så vill jag höra','här, så vill jag höra,'),
 ('Num',9,10):(', eller är ute på resa långt borta, och han ändå vill hålla HERRENS påskhögtid',' eller är ute på resa långt borta, och han ändå vill hålla HERRENS påskhögtid,'),
 ('Num',9,15):(' då tabernaklet sattes upp övertäckte molnskyn tabernaklet, vittnesbördets tält, och om aftonen, och sedan ända till morgonen, var det',', då tabernaklet sattes upp, övertäckte molnskyn tabernaklet, vittnesbördets tält, och om aftonen och sedan ända till morgonen var det,'),
 ('Num',9,16):('natten var det','natten var det,'),
 ('Num',9,17):('ställe','ställe,'),
 ('Num',9,19):(' vad HERREN bjöd dem iakttaga',', vad HERREN bjöd dem iakttaga,'),
 ('Num',9,20):('hända','hända,'),
 ('Num',9,21):('hända','hända,'),
 ('Num',9,22):(', eller en månad,',' eller en månad'),
 # Numbers 10
 ('Num',10,3):('dig,','dig'),
 ('Num',10,9):(', i edert land,',' i edert land'),
 ('Num',10,14):('första','första,'),
 ('Num',10,24):('fo','fö'),
 ('Num',10,29):(" om vilket HERREN har sagt: 'Det vill jag giva eder.' Följ du med oss, så vilja vi göra dig gott, ty HERREN har lovat Israel",", om vilket HERREN har sagt: 'Det vill jag giva eder.' Följ du med oss, så vilja vi göra dig gott, ty HERREN har lovat Israel,"),
 ('Num',10,31):('bäst','bäst,'),
 ('Num',10,32):('.','.»'),
 # Numbers 11
 ('Num',11,1):('förtärde de','förtärde dem'),
 ('Num',11,4):(' som åtföljde dem greps av lystnad; Israels barn själva begynte då ock',', som åtföljde dem, greps av lystnad; Israels barn själva begynte då också'),
 ('Num',11,5):('fisken','fisken,'),
 ('Num',11,6):('.','.»'),
 ('Num',11,8):(', och malde det därefter på handkvarn eller stötte sönder det i mortel,',' och malde det därefter på handkvarn eller stötte sönder det i mortel'),
 ('Num',11,10):('hörde','hörde,'),
 ('Num',11,12):('mig','mig,'),
 ('Num',11,21):('folk','folk,'),
 ('Num',11,22):('mängd','mängd,'),
 ('Num',11,23):(' se',' se,'),
 ('Num',11,24):('för folket','för folket,'),
 ('Num',11,25):('honom,','honom'),
 # Numbers 12
 ('Num',12,1):('skull','skull,'),
 ('Num',12,2):('ende','ende,'),
 ('Num',12,10):('han se','han se,'),
 # Numbers 13
 ('Num',13,16):(';',':'),
 ('Num',13,18):(',',''),
 ('Num',13,19):(' som bor däri',', som bor däri,'),
 ('Num',13,21):(' då de första druvorna voro mogna',', då de första druvorna voro mogna.'),
 ('Num',13,25):('skull','skull,'),
 ('Num',13,28):('land','land,'),
 ('Num',13,29):(' som bor i landet',', som bor i landet,'),
 ('Num',13,32):(' som hade varit däruppe med honom',', som hade varit däruppe med honom,'),
 ('Num',13,33):(' som vi hava genomvandrat och bespejat är ett land',', som vi hava genomvandrat och bespejat, är ett land,'),
 ('Num',13,34):(' att vi själva voro såsom gräshoppor, och sammalunda tyckte de om oss.',', att vi själva voro såsom gräshoppor, och sammalunda tyckte de om oss.»'),
 # Numbers 14
 ('Num',14,7):(' som vi hava genomvandrat och bespejat',', som vi hava genomvandrat och bespejat,'),
 ('Num',14,10):('ropade','ropade,'),
 ('Num',14,12):('dem,','dem;'),
 ('Num',14,13):('förnummit','förnummit,'),
 ('Num',14,14):('höra','höra,'),
 ('Num',14,17):('Herrens','HERRENS'),
 ('Num',14,19):('missgärning,','missgärning'),
 ('Num',14,23):('land','land,'),
 ('Num',14,24):('land','land,'),
 ('Num',14,25):('håll','håll,'),
 ('Num',14,27):('hört','hört,'),
 ('Num',14,28):('eder','eder,'),
 ('Num',14,29):('har','hav'),
 ('Num',14,31):('? dem skall jag låta komma ditin, och de skola lära känna det land',', dem skall jag låta komma ditin, och de skola lära känna det land,'),
 ('Num',14,33):('år,','år'),
 ('Num',14,34):(' vad det är',', vad det är,'),
 ('Num',14,36):('män','män,'),
 ('Num',14,37):(' som hade talat illa om landet',', som hade talat illa om landet,'),
 # Numbers 15
 ('Num',15,2):('eder,','eder'),
 ('Num',15,5):('brännoffer','brännoffer,'),
 ('Num',15,7):(' er',' en'),
 ('Num',15,8):('.',', så'),
 ('Num',15,9):('halv ','halv h'),
 ('Num',15,12):('ett','ett,'),
 ('Num',15,14):('främling','främling,'),
 ('Num',15,23):('HERRENS har bjudit eder genom Mose, från den dag då HERREN gav sina bud','HERREN har bjudit eder genom Mose, från den dag då HERREN gav sina bud,'),
 ('Num',15,26):('främlingen','främlingen,'),
 ('Num',15,34):('bestämt','bestämt,'),
 ('Num',15,38):('dem','dem,'),
 ('Num',15,39):('dem,','dem'),
 ('Num',15,40):('vill','vill,'),
 # Numbers 16
 ('Num',16,5):(' vem som hör honom till, och vem som är den helige',', vem som hör honom till, och vem som är den helige,'),
 ('Num',16,7):('man','man,'),
 ('Num',16,9):('nog','nog,'),
 ('Num',16,10):('Åt dig,','Åt dig'),
 ('Num',16,13):(' att du har fört oss hitupp ur ett land som flöt av mjölk och honung, för att låta oss dö I',', att du har fört oss hitupp ur ett land som flöt av mjölk och honung, för att låta oss dö i'),
 ('Num',16,14):('tror du','tror du,'),
 ('Num',16,17):('därpå,','därpå'),
 ('Num',16,18):(',',''),
 ('Num',16,20):('och H','Och H'),
 ('Num',16,22):('du Gud','du Gud,'),
 ('Num',16,28):(' att det är HERREN',', att det är HERREN,'),
 ('Num',16,30):('veta','veta,'),
 ('Num',16,35):('männen','männen,'),
 ('Num',16,38):('mäns','mäns,'),
 ('Num',16,39):('kopparfyrfaten','kopparfyrfaten,'),
 ('Num',16,40):('barn','barn,'),
 ('Num',16,41):('är I','är I,'),
 ('Num',16,47):('folket,','folket;'),
 # Numbers 17
 ('Num',17,2):('barn,','barn'),
 ('Num',17,5):(' att den man som jag utväljer, hans stav skall grönska. Och så skall jag göra slut på Israels barns knorrande, så att jag slipper höra',', att den man, som jag utväljer, hans stav skall grönska. Och så skall jag göra slut på Israels barns knorrande, så att jag slipper höra,'),
 ('Num',17,8):('hus,','hus;'),
 # Numbers 18
 ('Num',18,1):(', och din faders hus jämte dig, skolen bära den missgärning som vidlåder helgedomen; och du och dina söner jämte dig skolen bära den missgärning',' och din faders hus jämte dig skolen bära den missgärning, som vidlåder helgedomen; och du och dina söner jämte dig skolen bära den missgärning,'),
 ('Num',18,3):(' vad du har att iakttaga,',', vad du har att iakttaga'),
 ('Num',18,4):('och iakttaga','och iakttaga,'),
 ('Num',18,7):('iakttaga','iakttaga,'),
 ('Num',18,8):('giver åt dig','giver åt dig,'),
 ('Num',18,9):('! högheliga som icke lämnas åt elden: alla deras offergåvor, så ofta de frambära spisoffer eller syndoffer,',' högheliga som icke lämnas åt elden: alla deras offergåvor, så ofta de frambära spisoffer eller syndoffer'),
 ('Num',18,11):(' rad som skall tillhöra dig såsom en gärd av Israels barns gåvor, så ofta de frambära viftoffer; åt dig och åt dina söner och döttrar jämte dig giver jag det till en evärdlig rätt; var och en i ditt hus som är ren',', vad som skall tillhöra dig såsom en gärd av Israels barns gåvor, så ofta de frambära viftoffer; åt dig och åt dina söner och döttrar jämte dig giver jag det till en evärdlig rätt; var och en i ditt hus, som är ren,'),
 ('Num',18,13):(' som är ren',', som är ren,'),
 ('Num',18,15):('boskap','boskap,'),
 ('Num',18,19):('gåvor','gåvor,'),
 ('Num',18,20):('och H','Och H'),
 # Numbers 19
 ('Num',19,2):(' som HERREN har påbjudit: Säg till Israels barn att de skaffa fram till dig en röd, felfri ko, en som icke har något lyte,',', som HERREN har påbjudit: Säg till Israels barn, att de skaffa fram till dig en röd, felfri ko, en som icke har något lyte'),
 ('Num',19,4):(',',''),
 ('Num',19,6):('elden','elden,'),
 ('Num',19,8):(',',''),
 ('Num',19,9):(' som är ren skall samla ihop askan efter kon och lägga den utanför lägret på en ren plats. Den skall förvaras åt Israels barns menighet,',', som är ren, skall samla ihop askan efter kon och lägga den utanför lägret på en ren plats. Den skall förvaras åt Israels barns menighet'),
 ('Num',19,10):(' som samlade ihop askan efter kon',', som samlade ihop askan efter kon,'),
 ('Num',19,18):(' som är ren skall taga isop och doppa i vattnet och stänka på tältet och på allt bohaget,',', som är ren, skall taga isop och doppa i vattnet och stänka på tältet och på allt bohaget'),
 ('Num',19,19):(' som är ren',', som är ren,'),
 ('Num',19,21):(' som stänkte stänkelsevattnet',', som stänkte stänkelsevattnet,'),
 # Numbers 20
 ('Num',20,3):('bro','brö'),
 ('Num',20,8):(', och församla menigheten, du med din broder Aron, och talen till klippan inför deras ögon, så skall den giva vatten ifrån sig; så skaffar du fram vatten åt dem ur klippan och giver menigheten och dess boskap att dricka.',' och församla menigheten, du med din broder Aron, och talen till klippan inför deras ögon, så skall den giva vatten ifrån sig; så skaffar du fram vatten åt dem ur klippan och giver menigheten och dess boskap att dricka.»'),
 ('Num',20,9):('stav ','stav'),
 ('Num',20,14):('vedermödor','vedermödor,'),
 ('Num',20,16):('ängel','ängel,'),
 ('Num',20,17):('vingårdar,','vingårdar'),
 ('Num',20,20):(' n',' m'),
 ('Num',20,23):('talade till ','talade till Mose och '),
 ('Num',20,25):('dig,','dig'),
 ('Num',20,29):('förnam','förnam,'),
 # Numbers 21
 ('Num',21,1):('hörde','hörde,'),
 ('Num',21,4):(',',''),
 ('Num',21,7):('till HERREN','till HERREN,'),
 ('Num',21,14):('dalarna','dalarna,'),
 ('Num',21,16):('Beeri om brunnen där var det','Beer; om brunnen där var det,'),
 ('Num',21,18):('.','.»'),
 ('Num',21,19):(':',''),
 ('Num',21,20):(':',''),
 ('Num',21,22):('vingårdar,','vingårdar'),
 ('Num',21,32):('amoréerna','amoréerna,'),
 ('Num',21,33):('hål.','håll'),
 # Numbers 22
 ('Num',22,5):('folk','folk,'),
 ('Num',22,6):('vet','vet,'),
 ('Num',22,9):('män','män,'),
 ('Num',22,11):('folk','folk,'),
 ('Num',22,18):(' som hans hus rymmer, kunde jag dock icke överträda HERRENS min Guds',', som hans hus rymmer, kunde jag dock icke överträda HERRENS, min Guds,'),
 ('Num',22,19):('har över natten, för att jag må förnimma','här över natten, för att jag må förnimma,'),
 ('Num',22,27):('vrede','vrede,'),
 ('Num',22,30):('dig?','dig?»'),
 ('Num',22,33):('fo','fö'),
 ('Num',22,34):('icke','icke,'),
 ('Num',22,36):(' att Bileam kom, gick han honom till mötes till Ir i Moab, vid gränsen',', att Bileam kom, gick han honom till mötes till Ir i Moab, vid gränsen,'),
 # Numbers 23
 ('Num',23,2):(' såsom Bileam sade; och Balak och Bileam offrade en tjur och en vädur på vart altare',', såsom Bileam sade; och Balak och Bileam offrade en tjur och en vädur på vart altare.'),
 ('Num',23,3):(' »Stanna kvar vid ditt brännoffer; jag vill gå bort och se',': »Stanna kvar vid ditt brännoffer; jag vill gå bort och se,'),
 ('Num',23,5):('mun','mun,'),
 ('Num',23,7):('hör','hov'),
 ('Num',23,8):('gud ej förbannar,','Gud ej förbannar'),
 ('Num',23,9):('ett folk','ett folk,'),
 ('Num',23,12):(',',''),
 ('Num',23,15):('kvar ha','kvar hä'),
 ('Num',23,16):('mun','mun,'),
 ('Num',23,22):('Gud','Gud,'),
 ('Num',23,24):(', ett folk',', ett folk,'),
 ('Num',23,25):('.','.»'),
 ('Num',23,27):(' att du därifrån förbannar dem åt mig.',', att du därifrån förbannar dem åt mig.»'),
 ('Num',23,29):('.','.»'),
 # Numbers 24
 ('Num',24,1):(' att det var: HERRENS vilja att han skulle välsigna Israel, gick han icke, såsom de förra gångerna,',', att det var HERRENS vilja, att han skulle välsigna Israel, gick han icke såsom de förra gångerna'),
 ('Num',24,6):('dalar','dalar,'),
 ('Num',24,8):(' som har fört honom ut ur Egypten. Hans styrka är såsom en vildoxes. Han skall uppsluka de folk',', som har fört honom ut ur Egypten. Hans styrka är såsom en vildoxes. Han skall uppsluka de folk,'),
 ('Num',24,11):('tänkte','tänkte,'),
 ('Num',24,13):('guld','guld,'),
 ('Num',24,18):('få till besittning','få till besittning,'),
 ('Num',24,23):('han l','han h'),
 # Numbers 25
 ('Num',25,3):('Och f','och f'),
 ('Num',25,4):('huvudman,','huvudmän'),
 ('Num',25,13):(', och för hans avkomlingar efter honom, skall detta vara ett förbund',' och för hans avkomlingar efter honom skall detta vara ett förbund,'),
 ('Num',25,18):('dag','dag,'),
 # Numbers 26
 ('Num',26,7):(' som inmönstrades',', som inmönstrades,'),
 ('Num',26,18):('dem','dem,'),
 ('Num',26,22):('dem','dem,'),
 ('Num',26,25):('dem','dem,'),
 ('Num',26,27):('dem','dem,'),
 ('Num',26,28):('släkter','släkter,'),
 ('Num',26,34):(' som inmönstrades',', som inmönstrades,'),
 ('Num',26,36):('släkt','släkt.'),
 ('Num',26,37):(' som inmönstrades',', som inmönstrades,'),
 ('Num',26,41):(' som inmönstrades',', som inmönstrades,'),
 ('Num',26,43):('dem','dem,'),
 ('Num',26,47):('dem','dem,'),
 ('Num',26,50):(' som inmönstrades',', som inmönstrades,'),
 ('Num',26,51):('barn','barn,'),
 ('Num',26,54):(',',''),
 ('Num',26,56):('stam','stam,'),
 ('Num',26,57):(' som inmönstrades, efter deras släkter: Av Gerson gersoniternas släkt',', som inmönstrades, efter deras släkter: Av Gerson gersoniternas släkt,'),
 ('Num',26,58):(' maheliternas släkt, musiternas släkt',', maheliternas släkt, musiternas släkt,'),
 ('Num',26,62):(' som inmönstrades utgjorde tjugutre tusen, alla av mankön',', som inmönstrades, utgjorde tjugutre tusen, alla av mankön,'),
 ('Num',26,63):('hedar,','hedar'),
 # Numbers 27
 ('Num',27,4):('släkt','släkt,'),
 ('Num',27,5):('oc','Oc'),
 ('Num',27,11):('bro','brö'),
 ('Num',27,12):('land','land,'),
 ('Num',27,16):('Gud','Gud,'),
 ('Num',27,17):('får','får,'),
 ('Num',27,18):('man','man,'),
 ('Num',27,22):('gjorde','gjorde,'),
 # Numbers 28
 ('Num',28,13):('mjöl','mjöl,'),
 ('Num',28,17):(',',''),
 ('Num',28,27):('lamm,','lamm'),
 # Numbers 29
 ('Num',29,2):('lamm,','lamm'),
 ('Num',29,6):(', och förutom det dagliga brännoffret med tillhörande spisoffer,',' och förutom det dagliga brännoffret med tillhörande spisoffer'),
 ('Num',29,11):(',',''),
 ('Num',29,12):(',',''),
 # Numbers 30
 ('Num',30,2):('är','är,'),
 ('Num',30,3):('om någon gör ett löfte åt HERREN, eller svär en ed genom vilken han förbinder sig till återhållsamhet i något stycke, så skall han icke sedan bryta sitt ord; han skall i alla stycken göra','Om någon gör ett löfte åt HERREN eller svär en ed, genom vilken han förbinder sig till återhållsamhet i något stycke, så skall han icke sedan bryta sitt ord; han skall i alla stycken göra,'),
 ('Num',30,7):(', och löften då vila på henne,',' och löften då vila på henne'),
 ('Num',30,11):('löfte,','löfte'),
 ('Num',30,17):(' som HERREN av Mose angående förhållandet mellan en man och hans hustru,',', som HERREN gav Mose angående förhållandet mellan en man och hans hustru'),
 # Numbers 31
 ('Num',31,2):('barn ','barn av '),
 ('Num',31,10):(', i de trakter',' i de trakter,'),
 ('Num',31,11):('bytet,','bytet'),
 ('Num',31,16):('Peor,','Peor'),
 ('Num',31,17):('gossebarn,','gossebarn'),
 ('Num',31,18):('flickebarn','flickebarn,'),
 ('Num',31,19):('av eder','av eder,'),
 ('Num',31,21):('lagstadga','lagstadga,'),
 ('Num',31,28):(' som har varit med i striden',', som har varit med i striden,'),
 ('Num',31,30):('åligger att iakttaga','åligger att iakttaga,'),
 ('Num',31,31):('gjorde','gjorde,'),
 ('Num',31,32):('tagit','tagit,'),
 ('Num',31,34):(',',''),
 ('Num',31,35):('kvinnor','kvinnor,'),
 ('Num',31,36):(', eller den del om tillföll dem som hade varit med',' eller den del, som tillföll dem som hade varit med i'),
 ('Num',31,47):('ålåg att iakttaga','ålåg att iakttaga,'),
 ('Num',31,50):('del','det,'),
 ('Num',31,52):('guldet','guldet,'),
 # Numbers 32
 ('Num',32,1):('funno de','funno de,'),
 ('Num',32,4):('det land','det land,'),
 ('Num',32,5):('ögon','ögon,'),
 ('Num',32,7):(',',''),
 ('Num',32,11):('land','land,'),
 ('Num',32,13):(' som hade gjort',', som hade gjort,'),
 ('Num',32,16):('va','vå'),
 ('Num',32,19):('dem,','dem'),
 ('Num',32,20):('gören','gören,'),
 ('Num',32,22):('tillbaka','tillbaka,'),
 ('Num',32,24):(', och gårdar åt eder boskap, och gören',' och gårdar åt eder boskap, och gören,'),
 ('Num',32,25):('göra','göra,'),
 ('Num',32,26):('barn','barn,'),
 ('Num',32,32):(', och så få vår arvsbesittning ha',' och så få vår arvsbesittning hä'),
 ('Num',32,39):('amoréerna','amoréerna,'),
 ('Num',32,42):(', med underlydande orter,',' med underlydande orter'),
 # Numbers 33
 ('Num',33,35):('och d','Och d'),
 ('Num',33,40):('höra','höra,'),
 ('Num',33,54):(' där lotten bestämmer',', där lotten bestämmer,'),
 ('Num',33,55):(', och skola tränga eder i landet',' och skola tränga eder i landet,'),
 # Numbers 34
 ('Num',34,2):('det land','det land,'),
 ('Num',34,8):('dit','dit,'),
 ('Num',34,10):('oc','Oc'),
 ('Num',34,13):(' som I genom lottkastning skolen utskifta såsom arvedel åt eder, det land om vilket HERREN har bjudit',', som I genom lottkastning skolen utskifta såsom arvedel åt eder, det land, om vilket HERREN har bjudit,'),
 ('Num',34,20):('Samuel','Samuel,'),
 ('Num',34,24):('barn','barns'),
 # Numbers 35
 ('Num',35,2):('barn','barn,'),
 ('Num',35,6):(' som I given åt leviterna skola först och främst vara de sex fristäderna, vilka I skolen giva till det ändamålet',', som I given åt leviterna, skola först och främst vara de sex fristäderna, vilka I skolen giva till det ändamålet,'),
 ('Num',35,7):(' som I given åt leviterna',', som I given åt leviterna,'),
 ('Num',35,11):(' som ouppsåtligen har dödat någon',', som ouppsåtligen har dödat någon,'),
 ('Num',35,13):(' som I skolen giva till fristäder',', som I skolen giva till fristäder,'),
 ('Num',35,15):(' som bor ibland dem skola hava dessa sex städer såsom tillflyktsorter, till vilka var och er som ouppsåtligen har dödat någon',', som bor ibland dem, skola hava dessa sex städer såsom tillflyktsorter, till vilka var och en som ouppsåtligen har dödat någon,'),
 ('Num',35,17):('sten','sten,'),
 ('Num',35,18):('trä','trä,'),
 ('Num',35,20):(', eller med berått mod kastar något på honom;',' eller med berått mod kastar något på honom,'),
 ('Num',35,21):('på honom','på honom.'),
 ('Num',35,22):('annan,','annan'),
 ('Num',35,23):(' varmed dråpslag kan givas',', varmed dråpslag kan givas,'),
 ('Num',35,25):('fristaden','fristaden,'),
 ('Num',35,26):('fristad','fristad,'),
 ('Num',35,28):('ort','ort,'),
 ('Num',35,30):('nog','nog,'),
 ('Num',35,33):('det land','det land,'),
 ('Num',35,34):('landet','landet,'),
 # Numbers 36
 ('Num',36,2):('göra landet såsom arvedel åt Israels barn','giva landet såsom arvedel åt Israels barn,'),
 ('Num',36,6):('är','är,'),
 ('Num',36,7):(' som tillhör någon, av Israels barn',', som tillhör någon av Israels barn,'),
 ('Num',36,8):(' som inom någon av Israels barns stammar har kommit i besittning av en arvedel',', som inom någon av Israels barns stammar har kommit i besittning av en arvedel,')}

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
