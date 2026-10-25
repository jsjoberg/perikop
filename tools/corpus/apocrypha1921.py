#!/usr/bin/env -S uv run --locked
"""Derive resources/corpus/input/sv1921-apokryfer.tsv, the Swedish 1921 apocrypha.

Runeberg's 1921 facsimile OCR (runeberg-apokryf-1921-txt.zip) is the textual witness.
Its margin verse numbers are too unreliable for automatic segmentation, so the
Scrollmapper transcription (Swe1917.json), which derives from the same OCR, supplies
the verse skeleton. Every edit below was checked against the OCR page. With --check,
print remaining multi-word differences between the TSV and the OCR.
"""
import difflib, json, re, sys, zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
inputs=root/'resources/corpus/input'
OUTPUT=inputs/'sv1921-apokryfer.tsv'
# In the order of the 1921 edition.
BOOKS={'Judith':'Jdt','Wisdom':'Wis','Tobit':'Tob','Sirach':'Sir','Baruch':'Baruch','I Maccabees':'1Macc','II Maccabees':'2Macc','Esther (Greek)':'EsthGr','Susanna':'Sus','Bel and the Dragon':'Bel','Prayer of Azariah':'PrAzar','Prayer of Manasses':'PrMan'}

# Verse numbers that the transcription left inside the preceding verse: (book, chapter, verse, marker).
LEAKED=[('Tob',3,1,'2 »'),('Tob',7,16,'17 »'),('Tob',9,1,'2 »'),('Tob',10,4,'5 »'),('Jdt',2,4,'5 »'),('Jdt',6,1,'2 »'),('Jdt',6,18,'19 »'),
 ('Jdt',7,8,'9 »'),('Jdt',7,23,'24 »'),('Jdt',9,7,'8 »'),('Jdt',10,7,'8 »'),('Jdt',10,14,'15 »'),('Jdt',11,20,'21 »'),('Jdt',14,17,'18 »'),
 ('Baruch',2,28,'29’'),('Sus',1,28,'29 »'),('1Macc',5,25,'26 »'),('1Macc',7,16,'17 »'),('1Macc',7,27,'28 »'),('1Macc',7,36,'37 »'),
 ('1Macc',7,40,'41 »'),('1Macc',8,19,'20 »'),('1Macc',9,28,'29 »'),('1Macc',10,22,'23 »'),('1Macc',10,69,'70 »'),('1Macc',11,49,'50 »'),
 ('1Macc',13,14,'15 »'),('2Macc',3,37,'38 »'),('2Macc',6,23,'24 »'),('2Macc',7,5,'6 »'),('2Macc',7,21,'22 »'),('2Macc',8,17,'18 »'),
 ('2Macc',10,9,'10} '),('2Macc',14,34,'35 »'),('2Macc',15,15,'16 »'),('2Macc',14,4,'6 »'),('Wis',11,26,'12,1 ')]
# Verses split where an OCR margin number marks the boundary: (book, chapter, verse, first words of the next verse).
SPLIT=[('Sir',12,2,'Välgärningar må ej bevisas'),('Tob',11,18,'och Tobias’ bröllop'),('Tob',13,17,'och alla dess gränder'),('Wis',16,1,'Medan dessa så blevo straffade'),('Wis',16,10,'Ty för att de skulle'),('1Macc',10,45,'När Jonatan och folket'),
 ('1Macc',11,30,'En avskrift av det brev'),('2Macc',14,4,'Men sedan fick han')]
# Literal replacements inside one verse: (book, chapter, verse, old, new).
REPLACE=[
 # Printed page 106 (runeberg.org/apokryf/0110.html) has i, not OCR's l.
 ('Sir',28,9,'leva l frid','leva i frid'),
 # Printed page 62 (runeberg.org/apokryf/0066.html) has an exclamation mark.
 ('Tob',11,15,'min son TobiasI»','min son Tobias!»'),
 # Further scan-verified OCR errors: A003-A012 in docs/reports/sv1917-errata.md.
 ('Sir',4,8,'saktmod vänligr svar','saktmod vänligt svar'),
 ('Sir',13,10,'icke rnå bliva glömd','icke må bliva glömd'),
 ('Sir',17,23,'vedergälla dern','vedergälla dem'),
 ('Sir',34,9,'En väiuppfostrad man','En väluppfostrad man'),
 ('Sir',34,19,'skygd rnot den','skygd mot den'),
 ('Sir',50,26,'de sorn äro','de som äro'),
 ('1Macc',7,38,'låt dern icke','låt dem icke'),
 ('1Macc',15,41,'fotfolk, sorn skulle','fotfolk, som skulle'),
 ('2Macc',3,25,'sina f ramhovar','sina framhovar'),
 ('PrAzar',1,19,'handla rned oss','handla med oss'),
 # Third scan review: A013-A026 in docs/reports/sv1917-errata.md.
 ('Wis',18,16,'din oryggliga befällning','din oryggliga befallning'),
 ('Sir',7,8,'bliva osträffad','bliva ostraffad'),
 ('Sir',11,5,'rnen den som','men den som'),
 ('Sir',13,23,'»Vern är denne?','»Vem är denne?'),
 ('Sir',31,16,'väcka motvilja."','väcka motvilja.'),
 ('Sir',31,25,'de"som vinet','de som vinet'),
 ('Sir',37,3,'har da vällt fram','har du vällt fram'),
 ('Sir',44,9,'någon aminnelse','någon åminnelse'),
 ('Baruch',4,18,'som nar låtit','som har låtit'),
 ('1Macc',1,12,'fann behåg','fann behag'),
 ('1Macc',10,14,'Betsura stannåde','Betsura stannade'),
 ('1Macc',14,42,'insätta uppsyningsrnan','insätta uppsyningsmän'),
 ('2Macc',5,22,'kvar uppsyningsrnan','kvar uppsyningsmän'),
 ('2Macc',11,10,'framåt, ordnåde','framåt, ordnade'),
 # Complete-book scan review: A027-A030; chapter coverage is in the verification ledger.
 ('Bel',1,34,'i lejonssgropen','i lejongropen'),
 ('PrAzar',1,42,'i evighet','i evighet.'),
 ('Sus',1,20,'»Se,trädgårdsporten','»Se, trädgårdsporten'),
 ('Sus',1,24,'ropa med högröst','ropa med hög röst'),
 # Complete Judith scan review: A031-A036; printed med in 5:8 remains V003.
 ('Jdt',5,22,'allt folket som stod','allt folket, som stod'),
 ('Jdt',7,3,'till Gyamon,','till Cyamon,'),
 ('Jdt',7,30,'mina broder!','mina bröder!'),
 ('Jdt',8,5,'hade gjorts sig','hade gjort sig'),
 ('Jdt',10,3,'Manasses’livstid','Manasses’ livstid'),
 ('Jdt',14,15,'sängpallen;och','sängpallen; och'),
 # Complete Wisdom scan review: A037-A060, including two repaired verse boundaries.
 ('Wis',2,5,'lik ens skugga','lik en skugga'),
 ('Wis',2,9,'vår festgädje','vår festglädje'),
 ('Wis',2,17,'bliver för honom,','bliver för honom.'),
 ('Wis',2,24,'höra honom till, få erfara','höra honom till, de få erfara'),
 ('Wis',4,16,'skall, fästän död,','skall, fastän död,'),
 ('Wis',6,1,'I som domen jordens','I som dömen jordens'),
 ('Wis',7,23,'av fullkomlig finhet,','av fullkomlig finhet.'),
 ('Wis',8,3,'gör hon kunniga genom','gör hon kunnig genom'),
 ('Wis',8,11,'skarpsinnig i i domslut','skarpsinnig i domslut'),
 ('Wis',11,7,'barnen kulle dödas','barnen skulle dödas'),
 ('Wis',12,18,'närhelst du vill.','närhelst du vill,'),
 ('Wis',14,22,'mitt under i sitt elände','mitt under sitt elände'),
 ('Wis',14,19,'likhet; och så begynte mängden,','likhet;'),
 ('Wis',14,20,'som fängslades genom','och så begynte mängden, som fängslades genom'),
 ('Wis',16,1,'dessa i människor','dessa människor'),
 ('Wis',16,8,'att du är den','att du är den som frälsar ifrån allt ont.'),
 ('Wis',16,9,'som frälsar ifrån allt ont. Ty dessa','Ty dessa'),
 ('Wis',17,5,'st järnlågorna','stjärnlågorna'),
 ('Wis',17,15,'och Oförtänkt förskräckelse','och oförtänkt förskräckelse'),
 ('Wis',18,9,'endräk-tigt','endräktigt'),
 ('Wis',19,3,'jämrade Sigvid','jämrade sig vid'),
 ('Wis',19,7,'överskygga. lägret','överskygga lägret'),
 ('Wis',19,15,'ytterligare hemsökelser medan','ytterligare hemsökelse: medan'),
 ('Wis',19,17,'rättfärd iges dörr','rättfärdiges dörr'),
 # Complete Tobit scan review: A061-A080; printed orsdpråk in 3:4 remains V004.
 ('Tob',1,3,'utvandra till i Nineve','utvandra till Nineve'),
 ('Tob',1,4,'för evig tid,','för evig tid.'),
 ('Tob',4,6,'om du göre sanningen','om du gör sanningen'),
 ('Tob',4,14,'över nätten för','över natten för'),
 ('Tob',5,1,'svarade och sade honom','svarade och sade till honom'),
 ('Tob',5,11,'sade till i honom','sade till honom'),
 ('Tob',6,6,'tillsagt honom,;','tillsagt honom;'),
 ('Tob',6,18,'eder -barmhärtighet','eder barmhärtighet'),
 ('Tob',8,6,'Du sade:’Det','Du sade: ’Det'),
 ('Tob',8,19,'åt dem ett i bröllop','åt dem ett bröllop'),
 ('Tob',12,5,'Sedan kallades han till sig','Sedan kallade han till sig'),
 ('Tob',12,6,'Ja, våren icke sena','Ja, varen icke sena'),
 ('Tob',13,1,'en bön, enl fröjdesång','en bön, en fröjdesång'),
 ('Tob',14,10,'ochhuru illa han','och huru illa han'),
 ('Tob',14,12,'begrov i han henne','begrov han henne'),
 ('Tob',14,13,'sin fader Tobits,','sin fader Tobits.'),
 # Syrak 1-6 scan review: A081-A087; verses 3:26-27 are renumbered below.
 ('Sir',2,9,'vanten eder lycka','vänten eder lycka'),
 ('Sir',2,10,'åkallade honörn','åkallade honom'),
 ('Sir',3,14,'bliva förgäten i stället','bliva förgäten; i stället'),
 ('Sir',5,12,'handen på munnen,','handen på munnen.'),
 # Syrak 7-12 scan review: A088-A099; 12:3-4 are restored below.
 ('Sir', 7, 20, 'helt åt din tjänst,', 'helt åt din tjänst.'),
 ('Sir', 7, 23, 'allt iirån ungdomen', 'allt ifrån ungdomen'),
 ('Sir', 8, 8, 'huru du’; skall', 'huru du skall'),
 ('Sir', 8, 14, 'giva honom rätt,', 'giva honom rätt.'),
 ('Sir', 10, 9, 'den som stoft och aska', 'den som är stoft och aska'),
 ('Sir', 11, 4, 'din äras dag Ty', 'din äras dag. Ty'),
 ('Sir', 11, 11, 'och avlas,', 'och ävlas,'),
 ('Sir', 11, 30, 'den övermodiges hjärta,', 'den övermodiges hjärta, och lik spejaren spanar han efter ditt fall.'),
 ('Sir', 11, 31, 'och lik spejaren spanar han efter ditt fall. Försåtligt vänder han gott tills ont, och på det allra bästa sätter han en fläck.', 'Försåtligt vänder han gott till ont, och på det allra bästa sätter han en fläck.'),
 ('Jdt',1,1,'Konung Nebukadnessar bådar upp alla länder till krig mot konung Arfaksad. Länderna västerut vägra att hörsamma denna befallning. Arfaksad besegras. ',''),
 ('PrMan',1,1,'2 Krön 33,12; Syndabekännelse och bön om förlåtelse. ',''),
 ('Wis',3,2,', Mack 7,33; Hebr 12,7.',','),('Wis',9,18,' 1 Kor 2,16.',''),('Tob',4,21,' 16,2; 2 Kor 8,12.',''),
 ('Wis',11,2,' Därför straffar du först efter hand dem som falla och varnar dem, därmed att du påminner dem om det vari de synda, på det att de skola skilja sig ifrån sin ondska och tro på dig, o Herre.',''),
 ('Wis',16,10,'på-n minnas','påminnas'),
 ('Sus',1,1,' att de voro falska vittnen, och gjorde mot dem detsamma som de själva hade haft det onda uppsåtet att göra mot sin nästa,',''),
 ('Sus',1,61,'hade','hade visat att de voro falska vittnen, och gjorde mot dem detsamma som de själva hade haft det onda uppsåtet att göra mot sin nästa,'),
 ('Baruch',2,21,'skolen 1 få','skolen I få'),('Baruch',3,34,'glädja sig; 35','glädja sig;'),('Baruch',4,1,'Övergiva','övergiva'),
 ('Sir',1,6,'rådslag? [','rådslag?'),('1Macc',5,60,'av^','av'),('1Macc',13,2,'församlade]','församlade'),('1Macc',11,30,'brevst','brev'),
 ('2Macc',14,4,'tillfälles','tillfälle'),('EsthGr',4,2,'Orenlighet','orenlighet'),
 ('PrAzar',1,7,'tagit dem i akt','tagit dem i akt och ej gjort vad du har bjudit oss, för att det skulle gå oss väl.'),
 ('PrAzar',1,23,'upphörde icke att elda','upphörde icke att elda under den med bergolja och tjära och blår och torrt ris.')]
# Whole verses restored from the OCR, or replacing transcription debris.
SET={('Wis',1,16):'Men de ogudaktiga kalla döden till sig med ord och gärningar, de räkna honom för en vän och tråna efter honom. Ja, de hava slutit förbund med honom; de äro ju ock värda att höra honom till.',
 ('Wis',4,20):'När deras synder räknas samman, skola de bävande träda fram, och deras överträdelser skola vittna mot dem och överbevisa dem.',
 ('Wis',12,2):'Därför straffar du först efter hand dem som falla och varnar dem, därmed att du påminner dem om det vari de synda, på det att de skola skilja sig ifrån sin ondska och tro på dig, o Herre.',
 ('Sir',1,1):'All vishet kommer från Herren, och hos honom förbliver hon till evig tid.',
 ('Sir',4,31):'Låt icke din hand vara uträckt till att taga emot, men tillsluten, när du skall giva igen.',
 ('Sir',22,27):'Ack att jag hade en vakt för min mun och ett konstrikt insegel på mina läppar, så att jag icke komme på fall genom min tunga och icke genom henne råkade i fördärv!',
 ('Sir',26,29):'En köpman kommer svårligen undan försyndelser, och en krämare går icke fri ifrån synd.',
 ('Sir',27,30):'Agg och vrede, också de äro en styggelse, och en syndig man framhärdar i sådant.',
 ('PrAzar',1,1):'Men de tre männen gingo omkring mitt ibland lågorna och lovsjöngo Gud och prisade Herren.'}
APPEND={('Sir',38,34):' Icke så den som allenast tänker på den Högstes lag och begrundar den i sitt sinne.',
 ('Sir',44,23):' Och han lät en ättling uppstå av honom, en from man, vilken fann nåd för alla människors ögon'}
DELETE=[('Sir',1,7),('Wis',16,11)] # Bracket debris; a summary fragment (16:11 is restored by SPLIT).
# The transcription lost Bel 1-22 and scrambled the two-column pages of Tillägg till Ester 4:17-7:11.
BEL=['Och konung Astyages samlades till sina fäder, och persern Cyrus mottog riket efter honom.',
 'Och Daniel var en av konungens förtrogne och var mer hedrad än någon annan av hans vänner.',
 'Nu var det så, att babylonierna hade en avgud, vid namn Bel; och de bestodo honom dagligen tolv skäppor fint mjöl och fyrtio får och sex fat vin.',
 'Också konungen dyrkade Bel och begav sig dagligen åstad för att tillbedja honom. Men Daniel tillbad sin Gud. Då sade en gång konungen till honom: »Varför tillbeder du icke Bel?»',
 'Han svarade: »Därför att jag icke dyrkar avgudar som äro gjorda med händer, utan den levande Guden, som har skapat himmel och jord, och som är herre över allt levande.»',
 'Då sade konungen till honom: »Håller du icke Bel för att vara en levande gud? Eller ser du icke huru mycket han äter och dricker var dag?» Men Daniel log och sade:',
 '»Låt icke bedraga dig, o konung. Denne är ju innantill av lera och utantill av koppar och har aldrig ätit eller druckit något.»',
 'Då blev konungen förtörnad och lät kalla till sig hans präster och sade till dem: »Om I icke sägen mig vem som förtär allt detta som bestås Bel, så skolen I dö.',
 'Men om I kunnen visa mig att Bel förtär det, då skall Daniel dö, därför att han har hädat honom.» Och Daniel sade till konungen: »Må det bliva såsom du har sagt.»',
 'Men Bels präster voro sjuttio, förutom deras hustrur och barn. Så begav sig konungen, åtföljd av Daniel, till Bels tempel.',
 'Och Bels präster sade: »Nu gå vi ut härifrån. Men låt du, o konung, sätta fram maten, och låt blanda till vinet och ställa fram det; stäng så igen porten och försegla den med ditt signet.',
 'Kom sedan hit i morgon bittida, och om du då icke finner allt vara förtärt av Bel, så må vi dö, eller, i motsatt fall, Daniel, som har talat lögnaktigt om oss.»',
 'Men de kände ingen oro, ty de hade gjort en hemlig ingång under bordet, och genom denna plägade de alltid komma in och förtära det som var framsatt.',
 'Då nu dessa hade gått ut, lät konungen sätta fram maten åt Bel. Men Daniel befallde sina tjänare att hämta aska och lät dem strö ut den i hela templet, så att allenast konungen såg det. Sedan gingo de ut och stängde porten och förseglade den med konungens signet och begåvo sig därifrån.',
 'Men prästerna infunno sig under natten efter sin vana, tillika med sina hustrur och barn, och de åto och drucko upp alltsammans.',
 'Bittida följande morgon kom konungen dit, åtföljd av Daniel.',
 'Och han sade: »Äro sigillen orörda, Daniel?» Han svarade: »Ja, de äro orörda, o konung.»',
 'Strax då man öppnade porten, kastade konungen en blick på bordet och ropade med hög röst: »Stor är du, Bel, och hos dig finnes alls icke något svek.»',
 'Men Daniel log och höll konungen tillbaka, för att han icke skulle gå ditin, och sade: »Betrakta då golvet, och lägg märke till vems fotspår detta är.»',
 'Då sade konungen: »Jag ser fotspår efter män, kvinnor och barn.»',
 'Nu blev konungen vred och lät gripa prästerna och deras hustrur och barn. Och de måste visa honom den lönndörr genom vilken de plägade gå in för att förtära det som fanns på bordet.',
 'Och konungen lät avliva dem och gav Bel i Daniels våld. Och han förstörde honom själv och hans helgedom.']
ESTHER={(4,18):'Och din tjänarinna har icke känt någon glädje, allt ifrån den dag då jag kom hit intill nu, förutom över dig, Herre, Abrahams Gud.',
 (4,19):'O Gud, du som har makt över alla, hör deras röst, som hava uppgivit hoppet, och rädda oss ur ogärningsmännens hand; och befria mig från min fruktan.»',
 (5,1):'På tredje dagen, sedan hon hade upphört att bedja, tog hon av sig de kläder hon bar under sin bön och iförde sig sin praktskrud.',
 (5,13):'Då sade hon till honom: »När jag såg dig, herre, var det som om jag hade sett en Guds ängel; och jag förlorade fattningen av fruktan för din härlighet.',
 (5,15):'Men under det att hon så talade, signade hon åter ned av vanmakt.',
 (5,16):'Och konungen var upprörd, och alla hans tjänare sökte uppmuntra henne.',
 (6,1):'Vad som härnedan följer utgör en avskrift av brevet härom: »Den store konungen Artaxerxes hälsar landsfurstarna i de ett hundra tjugusju hövdingdömena, från Indien ända till Etiopien, så ock alla de andra som tjäna vår sak.',
 (6,5):'Ja, många av dem som hava blivit satta att härska hava ofta genom övertalning av vänner, vilka hava fått sig vården av rikets angelägenheter anförtrodd, gjorts delaktiga i utgjutandet av oskyldigt blod och invecklats i ohjälpliga olyckor,',
 (6,14):'Genom sådana anslag trodde han nämligen att han skulle kunna ställa oss ensamma och så få herraväldet över perserna överflyttat på macedonierna.',
 (6,15):'Men vi för vår del finna att judarna, som av denne store usling hava blivit prisgivna till att utrotas, alls icke äro några ogärningsmän, utan att de leva efter de allra rättfärdigaste lagar',
 (6,16):'och äro barn till den allrahögste och allrastörste levande Guden, som genom sin ledning, oss såväl som förut våra fäder till fromma, bevarar riket i det bästa tillstånd.',
 (6,17):"I gören alltså väl, om I icke vidare rätten eder efter den av Haman, Hamadatus' son, utsända skrivelsen.",
 (6,18):'Just den man som har kommit allt detta åstad har nämligen, tillika med hela sitt hus, blivit korsfäst utanför Susas portar, i det att den Gud som råder över allting skyndsamt har låtit honom få sin välförtjänta dom.',
 (6,19):'Och på alla platser skolen I öppet låta anslå en avskrift av detta brev, med innehåll att man skall tillstädja judarna att följa sina egna seder,',
 (6,20):'och att man skall bistå dem i att försvara sig mot dem som angripa dem i nödens stund, och detta just på trettonde dagen i tolfte månaden, det är Adar.',
 (6,21):'Ty denna dag har den Gud som härskar över allting bestämt till en glädjedag för det utvalda folket, i stället för till en undergångens dag.',
 (6,22):'Och I själva mån nu bland edra minneshögtider utmärka en särskild dag till att firas med all möjlig festglädje,',
 (6,23):'på det att den, såväl nu som framdeles, må för oss och de välsinnade perserna bliva en högtidsdag till hågkomst av vår räddning, men för dem som stämpla mot oss en påminnelse om undergång.',
 (6,24):'Och var stad eller vart hövdingdöme utan undantag, som icke rättar sig härefter, skall med all grymhet förhärjas med eld och svärd. De skola icke allenast göras otillgängliga för människofot, utan också för all framtid i högsta grad avskydda av vilda djur och fåglar.»',
 (7,1):'Och Mardokeus sade: »Från Gud har allt detta kommit.',
 (7,2):'Jag har nämligen dragit mig till minnes den dröm som jag hade om dessa tilldragelser; och det är intet därav, som icke har slagit in.',
 (7,3):'Där var en liten källa, som blev till en flod, och där var ljus och solsken och ett väldigt vatten. Floden, det är Ester, som konungen tog till äkta och gjorde till drottning.',
 (7,4):'Och de båda drakarna, det är jag och Haman.',
 (7,5):'Och folken, det är de som rotade sig samman för att utplåna judarnas namn.',
 (7,6):'Men mitt folk, det är Israel, som ropade till Gud och blev räddat. Ja, Herren har räddat sitt folk, och Herren har frälst oss ur alla dessa olyckor. Och Gud har gjort dessa stora tecken och under, sådana som icke hava förekommit bland hednafolken.',
 (7,7):'Fördenskull redde Gud till två lotter, en för sitt folk och en för alla hednafolk.',
 (7,8):'Och dessa båda lotter föllo ut på bestämd tid och stund, på den dag då dom blev hållen inför Gud över hans folk och alla hednafolk.',
 (7,9):'Och Gud tänkte på sitt folk och skaffade sin arvedel rätt.',
 (7,11):"I Ptolemeus' och Kleopatras fjärde regeringsår förde Dositeus, som uppgav sig vara präst och levit, jämte Ptolemeus, hans son, ovanstående brev om purimsfesten med sig hit. Och de sade att det var det rätta brevet om purimsfesten, och att det var översatt av Lysimakus, Ptolemeus' son, en man från Jerusalem."}

def derive():
    data=json.loads((inputs/'Swe1917.json').read_text())
    verses={}
    for book in data['books']:
        if book['name'] not in BOOKS:continue
        for chapter in book['chapters']:
            for verse in chapter['verses']:
                if verse['text'].strip():verses[(BOOKS[book['name']],chapter['chapter'],verse['verse'])]=verse['text']
    def once(key,old,new):
        if verses[key].count(old)!=1:raise ValueError('Edit does not apply once: %s %r'%(key,old))
        verses[key]=verses[key].replace(old,new)
    for book,chapter,verse,marker in LEAKED:
        text=verses[(book,chapter,verse)];i=text.index(marker);number=re.match(r'(\d+)(?:,(\d+))?',marker)
        target=(book,int(number[1]),int(number[2])) if number[2] else (book,chapter,int(number[1]))
        rest=text[i+len(number[0]):].lstrip('} ')
        if target in verses:raise ValueError('Leaked verse already present %s'%(target,))
        verses[(book,chapter,verse)]=text[:i].rstrip();verses[target]=rest
    for book,chapter,verse,old,new in REPLACE:once((book,chapter,verse),old,new)
    # A084-A086: the printed 3:25 is absent; these two texts are 3:26 and 3:27.
    for old,new,expected in ((26, 27, 'Ett förmätet hjärta bliver nedtyngt av vedermödor, och syndaren hopar synd på synd.'), (25, 26, 'Ett förmätet hjärta går det på sistone illa, och den som gärna giver sig i fara, han förgås däri.')):
        key,target=('Sir',3,old),('Sir',3,new)
        if target in verses or verses.get(key)!=expected:raise ValueError('Syrak 3:26-27 source changed')
        verses[target]=verses.pop(key)
    # A097-A099: the printed 12:3 is attached to 12:2; free its slot before splitting.
    key,target=('Sir',12,3),('Sir',12,4)
    if target in verses or verses.get(key)!='Giv åt den som är god, men tag dig ej an syndaren.':raise ValueError('Syrak 12:4 source changed')
    verses[target]=verses.pop(key)
    for key in DELETE:del verses[key]
    verses[('1Macc',10,45)]=verses.pop(('1Macc',10,46)) # 10:45-46 share one slot.
    for book,chapter,verse,start in SPLIT:
        text=verses[(book,chapter,verse)];i=text.index(start)
        target=(book,chapter,verse+1)
        if target in verses:raise ValueError('Split target present %s'%(target,))
        verses[(book,chapter,verse)]=text[:i].rstrip();verses[target]=text[i:]
    verses.update(SET)
    for key,text in APPEND.items():verses[key]+=text
    for verse,text in enumerate(BEL,1):verses[('Bel',1,verse)]=text
    for key in [k for k in verses if k[0]=='EsthGr' and k[1]==7]:del verses[key]
    verses[('EsthGr',7,10)]='Och dessa dagar skola de fira i månaden Adar, på fjortonde och femtonde dagen i samma månad, med festförsamling och fröjd och glädje inför Gud, släkte efter släkte, i evärdlig tid, bland hans folk Israel.»'
    for (chapter,verse),text in ESTHER.items():verses[('EsthGr',chapter,verse)]=text
    order=list(BOOKS.values())
    rows=sorted(verses.items(),key=lambda item:(order.index(item[0][0]),item[0][1],item[0][2]))
    for (book,chapter,verse),text in rows:
        text=' '.join(text.split())
        if not text or re.search(r'\d+,\d+|[\[\]<>^{}]',text):raise ValueError('Unclean text %s %d:%d'%(book,chapter,verse))
        yield book,chapter,verse,text

def ocr_words():
    """Word stream per book from the OCR, without headings, notes and references, for --check."""
    archive=zipfile.ZipFile(inputs/'runeberg-apokryf-1921-txt.zip')
    text=''.join(archive.read('Pages/%04d.txt'%n).decode('utf-8').replace('\r','')+'\n' for n in range(5,267))
    text=re.sub(r'<chapter name="[^"]*">|</chapter>|<footnote>.*?</footnote>|<sub>.*?</sub>|</?(?:b|i|sp|h2)>','\n',text,flags=re.S)
    lines=[l for l in text.split('\n') if not l.startswith('<tab>') and not re.search(r'\d+:\s*\d+|/\s*[\dlIS]+[.\-]',l)]
    return re.findall(r"[A-Za-zÅÄÖåäöÉéÜü]+",re.sub(r'-\n','','\n'.join(lines)))

if __name__=='__main__':
    rows=list(derive())
    if '--check' in sys.argv:
        ocr=[w.lower() for w in ocr_words()]
        mine=[w.lower() for row in rows for w in re.findall(r"[A-Za-zÅÄÖåäöÉéÜü]+",row[3])]
        matcher=difflib.SequenceMatcher(None,mine,ocr,autojunk=False)
        for tag,i1,i2,j1,j2 in matcher.get_opcodes():
            if tag!='equal' and i2-i1>=3:print(tag,' '.join(mine[i1:i2]),'|',' '.join(ocr[j1:j2]))
    else:
        OUTPUT.write_text(''.join('%s\t%d\t%d\t%s\n'%row for row in rows))
        print('Wrote',len(rows),'verses to',OUTPUT.relative_to(root))
