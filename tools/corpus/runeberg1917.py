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
 # Leviticus 1 (EFS 1927 GT s. 138-139 and EFS 1926 GT s. 126-127): L001-L001, readings both prints agree on.
 ('Lev',1,11):('ansikte,','ansikte;'),
 # Leviticus 2 (EFS 1927 GT s. 139-140 and EFS 1926 GT s. 127-127): L002-L007, readings both prints agree on.
 ('Lev',2,1):('HERREN','HERREN,'),
 ('Lev',2,5):('spisoffer ','spisoffer, '),
 ('Lev',2,7):('spisoffer ','spisoffer, '),
 ('Lev',2,8):('spisoffer som är tillrett på något av dessa sätt','spisoffer, som är tillrett på något av dessa sätt,'),
 ('Lev',2,9):('del som utgör själva altaroffret','del, som utgör själva altaroffret,'),
 ('Lev',2,11):('spisoffer som I viljen bära fram åt HERREN','spisoffer, som I viljen bära fram åt HERREN,'),
 ('Lev',2,14):('böra fram','bära fram'),
 ('Lev',2,16):('Herren.','HERREN.'),
 # Leviticus 3 (EFS 1927 GT s. 140-140 and EFS 1926 GT s. 127-128): L008-L013, readings both prints agree on.
 ('Lev',3,3):('Herren bära fram det fett','HERREN bära fram det fett,'),
 ('Lev',3,5):('för Herren.','för HERREN.'),
 ('Lev',3,6):('djur,','djur'),
 ('Lev',3,7):('får','får,'),
 ('Lev',3,9):('fett som omsluter inälvorna, och allt det fett','fett, som omsluter inälvorna, och allt det fett,'),
 ('Lev',3,14):('fett som omsluter inälvorna, och allt det fett','fett, som omsluter inälvorna, och allt det fett,'),
 # Leviticus 4 (EFS 1927 GT s. 140-142 and EFS 1926 GT s. 128-130): L014-L026, readings both prints agree on.
 ('Lev',4,8):('fett som omsluter inälvorna, och allt det fett','fett, som omsluter inälvorna, och allt det fett,'),
 ('Lev',4,11):('fötter','fötter,'),
 ('Lev',4,12):('plats där man slår ut askan','plats, där man slår ut askan,'),
 ('Lev',4,13):('Herrens bud genom vilket något förbjudes','HERRENS bud genom vilket något förbjudes,'),
 ('Lev',4,15):('Herrens','HERRENS'),
 ('Lev',4,22):('bud genom vilket något förbjudes, och han själv märker','bud, genom vilket något förbjudes, och han själv märker,'),
 ('Lev',4,23):('veta','veta,'),
 ('Lev',4,24):('plats','plats,'),
 ('Lev',4,27):('bud genom vilket något förbjudes, och han själv märker','bud, genom vilket något förbjudes, och han själv märker,'),
 ('Lev',4,28):('veta vilken synd han har begått, så skall han,','veta, vilken synd han har begått, så skall han'),
 ('Lev',4,29):('plats','plats,'),
 ('Lev',4,31):('altaret,','altaret'),
 ('Lev',4,33):('plats ','plats, '),
 ('Lev',4,35):('Herrens eldsoffer.','HERRENS eldsoffer.'),
 # Leviticus 5 (EFS 1927 GT s. 142-144 and EFS 1926 GT s. 130-131): L027-L032, readings both prints agree on.
 ('Lev',5,2):('vilddjur, eller den döda kroppen av ett orent boskapsdjur,','vilddjur eller den döda kroppen av ett orent boskapsdjur'),
 ('Lev',5,6):('honom,','honom'),
 ('Lev',5,7):('Herren','HERREN'),
 ('Lev',5,10):('honom,','honom'),
 ('Lev',5,15):('Herren,','HERREN,'),
 ('Lev',5,17):('bud','bud,'),
 # Leviticus 6 (EFS 1927 GT s. 144-145 and EFS 1926 GT s. 131-132): L033-L043, readings both prints agree on.
 ('Lev',6,2):('hand,','hand'),
 ('Lev',6,4):('Och därmed ådragit sig skuld återställa vad han med våld har tagit eller med orätt tillägnat sig, eller det som har varit honom ombetrott, eller det borttappade','och därmed ådragit sig skuld återställa vad han med våld har tagit eller med orätt tillägnat sig eller det som har varit honom ombetrott, eller det borttappade,'),
 ('Lev',6,8):('Herren','HERREN'),
 ('Lev',6,10):('askan','askan,'),
 ('Lev',6,14):('ansikte,','ansikte'),
 ('Lev',6,15):('rökelsen som ligger på spisoffret, och detta, som utgör själva altaroffret, skall han förbränna på altaret,','rökelsen, som ligger på spisoffret, och detta, som utgör själva altaroffret, skall han förbränna på altaret'),
 ('Lev',6,18):('eldsoffer,','eldsoffer'),
 ('Lev',6,20):(' som Aron och hans söner skola offra åt HERREN på den dag',', som Aron och hans söner skola offra åt HERREN på den dag,'),
 ('Lev',6,25):('plats där brännoffersdjuret slaktas','plats, där brännoffersdjuret slaktas,'),
 ('Lev',6,26):(' som offrar syndoffret',', som offrar syndoffret,'),
 ('Lev',6,28):('lerkärl vari kokningen har skett','lerkärl, vari kokningen har skett,'),
 ('Lev',6,30):(' av vars blod något bäres in i uppenbarelsetältet till att bringa försoning i helgedomen',', av vars blod något bäres in i uppenbarelsetältet till att bringa försoning i helgedomen,'),
 # Leviticus 7 (EFS 1927 GT s. 145-147 and EFS 1926 GT s. 133-134): L044-L055, readings both prints agree on.
 ('Lev',7,2):('plats där man slaktar brännoffersdjuret','plats, där man slaktar brännoffersdjuret,'),
 ('Lev',7,3):('det fett','det fett,'),
 ('Lev',7,7):('präst','präst,'),
 ('Lev',7,8):('brännoffer for','brännoffer för'),
 ('Lev',7,9):('spisoffer som är bakat i ugn, eller som är tillrett i panna eller på plåt, skall alltid tillfalla den präst','spisoffer, som är bakat i ugn, eller som är tillrett i panna eller på plåt, skall alltid tillfalla den präst,'),
 ('Lev',7,10):('spisoffer','spisoffer,'),
 ('Lev',7,13):('lov offer.','lovoffer.'),
 ('Lev',7,14):('slag, såsom en gärd åt HERREN; den präst','slag såsom en gärd åt HERREN; den präst,'),
 ('Lev',7,16):('slaktoffer som någon vill bära fram år ett','slaktoffer, som någon vill bära fram, är ett'),
 ('Lev',7,21):('orenhet, eller ett orent djur,','orenhet eller ett orent djur'),
 ('Lev',7,25):('djur varav man bär fram eldsoffer åt HERREN, vem det vara må','djur, varav man bär fram eldsoffer åt HERREN, vem det vara må,'),
 ('Lev',7,32):('prästen,','prästen'),
 ('Lev',7,35):(' lott ',' lott, '),
 ('Lev',7,36):(', efter HERRENS befallning på den dag',' efter HERRENS befallning på den dag,'),
 ('Lev',7,38):(', på den dag då han bjöd Israels barn',' på den dag, då han bjöd Israels barn,'),
 # Leviticus 8 (EFS 1927 GT s. 147-149 and EFS 1926 GT s. 134-136): L056-L060, readings both prints agree on.
 ('Lev',8,4):('gjorde','gjorde,'),
 ('Lev',8,11):('fotställning,','fotställning'),
 ('Lev',8,16):('fett ','fett, '),
 ('Lev',8,17):('lägret','lägret,'),
 ('Lev',8,21):(', S',', s'),
 ('Lev',8,25):('det fett','det fett,'),
 ('Lev',8,32):('köttet;','köttet'),
 ('Lev',8,35):('iakttaga ','iakttaga, '),
 # Leviticus 9 (EFS 1927 GT s. 149-150 and EFS 1926 GT s. 136-139): L061-L064, readings both prints agree on.
 ('Lev',9,6):('»Detta är','»Detta är,'),
 ('Lev',9,7):('brännoffer,','brännoffer'),
 ('Lev',9,13):('förbrände det på','förbrände detta på'),
 ('Lev',9,15):('syndoffer,','syndoffer'),
 # Leviticus 10 (EFS 1927 GT s. 150-151 and EFS 1926 GT s. 139-139): L065-L069, readings both prints agree on.
 ('Lev',10,3):('»Detta är','»Detta är,'),
 ('Lev',10,4):('fören den utanför','fören dem utanför'),
 ('Lev',10,7):('gjorde','gjorde,'),
 ('Lev',10,11):('stadgar','stadgar,'),
 ('Lev',10,12):('spisoffer ','spisoffer, '),
 ('Lev',10,14):('dig, och av dina söner och dina döttrar jämte dig,','dig och av dina söner och dina döttrar jämte dig'),
 ('Lev',10,19):('vederfarits ','vederfarits, '),
 # Leviticus 11 (EFS 1927 GT s. 151-153 and EFS 1926 GT s. 139-140): L070-L081, readings both prints agree on.
 ('Lev',11,2):('de djur','de djur,'),
 ('Lev',11,3):(' som hava klövar och hava dem helkluvna,',', som hava klövar och hava dem helkluvna'),
 ('Lev',11,5):('fo','fö'),
 ('Lev',11,20):('smådjur som gå på fyra fötter','smådjur, som gå på fyra fötter,'),
 ('Lev',11,23):(' som hava fyra fötter',', som hava fyra fötter,'),
 ('Lev',11,26):('fyrfotadjur','fyrfotadjur,'),
 ('Lev',11,27):('djur som gå på tassar','djur, som gå på tassar,'),
 ('Lev',11,32):('allt varpå något sådant djur faller, sedan det är dött, bliver orent, vare sig det är något slags träkärl, eller det är kläder, eller något av skinn, eller en säck,','allt, varpå något sådant djur faller, sedan det är dött, bliver orent, vare sig det är något slags träkärl, eller det är kläder eller något av skinn eller en säck'),
 ('Lev',11,35):('allt varpå någon sådan död kropp faller','allt, varpå någon sådan död kropp faller,'),
 ('Lev',11,36):('plats','plats,'),
 ('Lev',11,39):('fyrfotadjur som får ätas av eder','fyrfotadjur, som får ätas av eder,'),
 ('Lev',11,41):('smådjur som röra sig på jorden','smådjur, som röra sig på jorden,'),
 ('Lev',11,46):(', och om fåglarna,',' och om fåglarna'),
 # Leviticus 12 (EFS 1927 GT s. 153-154 and EFS 1926 GT s. 140-140): L082-L083, readings both prints agree on.
 ('Lev',12,2):('gossebarn','gossebarn,'),
 ('Lev',12,5):('flickebarn','flickebarn,'),
 ('Lev',12,6):('brännoffer, ','brännoffer '),
 ('Lev',12,7):('gossebarn, ','gossebarn '),
 # Leviticus 13 (EFS 1927 GT s. 154-157 and EFS 1926 GT s. 140-143): L084-L117, readings both prints agree on.
 ('Lev',13,2):('fläck, ','fläck '),
 ('Lev',13,3):('finner att håret på det angripna stället har vitnat, och att det angripna stället visar sig djupare än den övriga huden på kroppen, så är han angripen av spetälska; och sedan prästen har besett honom','finner, att håret på det angripna stället har vitnat, och att det angripna stället visar sig djupare än den övriga huden på kroppen, så är han angripen av spetälska; och sedan prästen har besett honom,'),
 ('Lev',13,4):('fläck som synes på hans kropps hud, men den icke visar sig djupare än den övriga huden,','fläck, som synes på hans kropps hud, men den icke visar sig djupare än den övriga huden'),
 ('Lev',13,5):('finner','finner,'),
 ('Lev',13,6):('finner','finner,'),
 ('Lev',13,8):('finner','finner,'),
 ('Lev',13,10):('huden, och ser','huden och ser,'),
 ('Lev',13,13):('finner','finner,'),
 ('Lev',13,17):(' att det angripna stället har blivit vitt, så skall prästen förklara den angripne ren,',', att det angripna stället har blivit vitt, så skall prästen förklara den angripne ren;'),
 ('Lev',13,18):('bulnad ','bulnad, '),
 ('Lev',13,19):('sedan, på det ställe','sedan på det ställe,'),
 ('Lev',13,20):('finner att stället visar sig lägre än den övriga huden, och att håret därpå har vitnat, så skall prästen förklara honom oren; ty då är han angripen av spetälska, som har brutit ut','finner, att stället visar sig lägre än den övriga huden, och att håret därpå har vitnat, så skall prästen förklara honom oren; ty då är han angripen av spetälska, som har brutit ut,'),
 ('Lev',13,21):('finner','finner,'),
 ('Lev',13,23):('oförändrad där den är','oförändrad, där den är,'),
 ('Lev',13,25):('finner att håret på fläcken har vitnat, och att den visar sig djupare än den övriga huden, så är mannen angripen av spetälska, som har brutit ut','finner, att håret på fläcken har vitnat, och att den visar sig djupare än den övriga huden, så är mannen angripen av spetälska, som har brutit ut,'),
 ('Lev',13,26):('finner','finner,'),
 ('Lev',13,27):('finner att det onda har utbrett sig på huden, så skall prästen förklara honom oren','finner, att det onda har utbrett sig på huden, så skall prästen förklara honom oren,'),
 ('Lev',13,28):('oförändrad där den är','oförändrad, där den är,'),
 ('Lev',13,30):(' att det visar sig djupare än den övriga huden och att gulaktigt',', att det visar sig djupare än den övriga huden och att gulaktigt,'),
 ('Lev',13,32):('finner','finner,'),
 ('Lev',13,34):('finner','finner,'),
 ('Lev',13,36):('finner att skorven har utbrett sig på huden, så behöver prästen icke efterforska','finner, att skorven har utbrett sig på huden, så behöver prästen icke efterforska,'),
 ('Lev',13,37):('oförändrad,','oförändrad'),
 ('Lev',13,39):('finner','finner,'),
 ('Lev',13,42):('ut på det skalliga stället','ut på det skalliga stället,'),
 ('Lev',13,43):('finner att den upphöjda fläcken på det skalliga stället, baktill eller framtill, är rödvit,','finner, att den upphöjda fläcken på det skalliga stället, baktill eller framtill, är rödvit'),
 ('Lev',13,50):('inlåst i','inläst i'),
 ('Lev',13,51):('finner att skadan har utbrett sig på klädnaden, eller på det vävda eller virkade tyget,','finner, att skadan har utbrett sig på klädnaden eller på det vävda eller virkade tyget'),
 ('Lev',13,52):('klädnaden, eller det vävda eller virkade tyget, vare sig det år av ylle eller av linne, eller skinnsaken','klädnaden eller det vävda eller virkade tyget, vare sig det är av ylle eller av linne, eller skinnsaken,'),
 ('Lev',13,53):('finner att fläcken icke har utbrett sig på klädnaden, eller på det vävda eller virkade tyget,','finner, att fläcken icke har utbrett sig på klädnaden eller på det vävda eller virkade tyget'),
 ('Lev',13,54):('bjuda att man tvår den sak på vilken det angripna stället finnes, och han skall för andra gången hava den inlåst i','bjuda, att man tvår den sak, på vilken det angripna stället finnes, och han skall för andra gången hava den inläst i'),
 ('Lev',13,55):('finner','finner,'),
 ('Lev',13,56):('finner att det efter tvagningen har bleknat, så skall han riva bort det från klädnaden eller skinnet,','finner, att det efter tvagningen har bleknat, så skall han riva bort det från klädnaden eller skinnet'),
 ('Lev',13,57):('klädnaden, eller på det vävda eller virkade tyget, eller på skinnsaken, vad det vara må, så är det spetälska som har brutit ut; den sak på vilken det angripna stället finnes','klädnaden eller på det vävda eller virkade tyget eller på skinnsaken, vad det vara må, så är det spetälska, som har brutit ut; den sak, på vilken det angripna stället finnes,'),
 ('Lev',13,58):('klädnaden,','klädnaden'),
 ('Lev',13,59):('tyg, eller någon skinnsak, vad det vara må – den lag','tyg eller någon skinnsak, vad det vara må – den lag,'),
 # Leviticus 14 (EFS 1927 GT s. 157-160 and EFS 1926 GT s. 143-146): L118-L133, readings both prints agree on.
 ('Lev',14,3):('finner att han är botad från den spetälska','finner, att han är botad från den spetälska,'),
 ('Lev',14,4):('bjuda','bjuda,'),
 ('Lev',14,5):('bjuda','bjuda,'),
 ('Lev',14,7):('slappa','släppa'),
 ('Lev',14,11):('prästen som förrättar reningen','prästen, som förrättar reningen,'),
 ('Lev',14,13):('samma plats','samma plats,'),
 ('Lev',14,21):('skuldoffer, och vifta det för att bringa försoning för sig,','skuldoffer och vifta det för att bringa försoning för sig'),
 ('Lev',14,31):(':',','),
 ('Lev',14,35):('synes','synes,'),
 ('Lev',14,36):('bjuda','bjuda,'),
 ('Lev',14,37):('finner','finner,'),
 ('Lev',14,39):('finner','finner,'),
 ('Lev',14,40):('bjuda att man bryter ut de stenar','bjuda, att man bryter ut de stenar,'),
 ('Lev',14,44):('finner att fläcken har utbrett sig på huset, så är detta en elakartad spetälska på huset,','finner, att fläcken har utbrett sig på huset, så är detta en elakartad spetälska på huset;'),
 ('Lev',14,45):('huset, med dess stenar och trävirke och allt murbruk på huset,','huset med dess stenar och trävirke och allt murbruk på huset'),
 ('Lev',14,48):('finner','finner,'),
 ('Lev',14,51):('fågeln, och doppa alltsammans i den slaktade fågelns blod och det friska vattnet,','fågeln och doppa alltsammans i den slaktade fågelns blod och det friska vattnet'),
 # Leviticus 15 (EFS 1927 GT s. 160-161 and EFS 1926 GT s. 146-147): L134-L146, readings both prints agree on.
 ('Lev',15,4):(' varpå den sjuke ligger bliver orent, och allt varpå han sitter',', varpå den sjuke ligger, bliver orent, och allt, varpå han sitter,'),
 ('Lev',15,5):(' varpå han har legat',', varpå han har legat,'),
 ('Lev',15,6):('något varpå den sjuke har suttit','något, varpå den sjuke har suttit,'),
 ('Lev',15,9):(' varpå den sjuke sitter',', varpå den sjuke sitter,'),
 ('Lev',15,10):('honom','honom,'),
 ('Lev',15,12):('lerkärl som den sjuke kommer vid','lerkärl, som den sjuke kommer vid,'),
 ('Lev',15,13):('han, för att förklaras ren,','han för att förklaras ren'),
 ('Lev',15,20):(' varpå hon ligger under sin månadsrening bliver orent, och allt varpå hon sitter',', varpå hon ligger under sin månadsrening, bliver orent, och allt, varpå hon sitter,'),
 ('Lev',15,21):('det varpå hon har legat','det, varpå hon har legat,'),
 ('Lev',15,22):('något varpå hon har suttit','något, varpå hon har suttit,'),
 ('Lev',15,23):('det','det,'),
 ('Lev',15,24):(', och något av hennes månadsflöde kommer på honom, skall han vara oren i sju dagar, och allt varpå han ligger',' och något av hennes månadsflöde kommer på honom, skall han vara oren i sju dagar, och allt, varpå han ligger,'),
 ('Lev',15,26):(' varpå hon ligger, så länge hennes flöde varar, skall gälla detsamma som om det varpå hon ligger under sin månadsrening; och allt varpå hon sitter',', varpå hon ligger, så länge hennes flöde varar, skall gälla detsamma som om det, varpå hon ligger under sin månadsrening; och allt, varpå hon sitter,'),
 # Leviticus 16 (EFS 1927 GT s. 162-163 and EFS 1926 GT s. 147-149): L147-L160, readings both prints agree on.
 ('Lev',16,2):('Aron','Aron,'),
 ('Lev',16,9):('bock som lotten bestämmer åt HERREN','bock, som lotten bestämmer åt HERREN,'),
 ('Lev',16,10):('bock som lotten bestämmer åt Asasel','bock, som lotten bestämmer åt Asasel,'),
 ('Lev',16,11):('hus,','hus;'),
 ('Lev',16,12):('fyrfat','fyrfat,'),
 ('Lev',16,15):('blod såsom han gjorde med tjurens blod: han skall tänka','blod, såsom han gjorde med tjurens blod: han skall stänka'),
 ('Lev',16,16):('göra ned uppenbarelsetältet,','göra med uppenbarelsetältet,'),
 ('Lev',16,17):(', från den stund p',' från den stund d'),
 ('Lev',16,21):('huvud,','huvud'),
 ('Lev',16,23):('iklätt sig','iklätt sig,'),
 ('Lev',16,29):('främlingen','främlingen,'),
 ('Lev',16,30):('eder,','eder'),
 ('Lev',16,32):('ställe','ställe,'),
 ('Lev',16,34):('barn, till rening från alla deras synder, en gång om året. Och han gjorde','barn till rening från alla deras synder, en gång om året. Och han gjorde,'),
 # Leviticus 17 (EFS 1927 GT s. 163-164 and EFS 1926 GT s. 149-150): L161-L168, readings both prints agree on.
 ('Lev',17,2):('dem Detta är','dem: Detta är,'),
 ('Lev',17,3):('hus, i lägret eller utanför lägret,','hus i lägret eller utanför lägret'),
 ('Lev',17,6):('altare, vid ingången till uppenbarelsetältet,','altare vid ingången till uppenbarelsetältet'),
 ('Lev',17,8):('hus,','hus'),
 ('Lev',17,10):('hus,','hus'),
 ('Lev',17,12):('främlingen som bor ibland eder','främlingen, som bor ibland eder,'),
 ('Lev',17,13):('barn,','barn'),
 ('Lev',17,16):('kropp','kropp,'),
 # Leviticus 18 (EFS 1927 GT s. 164-166 and EFS 1926 GT s. 150-151): L169-L177, readings both prints agree on.
 ('Lev',18,3):('göra såsom man gör i Egyptens land, där I haven bott. Ej heller skolen I göra','göra, såsom man gör i Egyptens land, där I haven bott. Ej heller skolen I göra,'),
 ('Lev',18,4):('göra och mina stadgar skolen I hålla,','göra, och mina stadgar skolen I hålla'),
 ('Lev',18,5):('efter dem','efter dem,'),
 ('Lev',18,6):('som år hans','som är hans'),
 ('Lev',18,22):('en man','en man,'),
 ('Lev',18,23):('där med','dig med'),
 ('Lev',18,26):('främling','främling,'),
 ('Lev',18,28):('folk','folk,'),
 ('Lev',18,30):('därför','därför,'),
 # Leviticus 19 (EFS 1927 GT s. 166-167 and EFS 1926 GT s. 151-153): L178-L181, readings both prints agree on.
 ('Lev',19,5):('sätt','sätt,'),
 ('Lev',19,13):('något,','något'),
 ('Lev',19,14):('något','något,'),
 ('Lev',19,20):('henne, och hon är trälinna i en annan mans våld,','henne och hon är trälinna i en annan mans våld'),
 ('Lev',19,34):(' som bor hos eder',', som bor hos eder,'),
 # Leviticus 20 (EFS 1927 GT s. 168-169 and EFS 1926 GT s. 153-154): L182-L192, readings both prints agree on.
 ('Lev',20,2):('barn,','barn'),
 ('Lev',20,3):('Molok,','Molok'),
 ('Lev',20,6):('andar','andar,'),
 ('Lev',20,7):('heliga,','heliga'),
 ('Lev',20,10):('kvinnan','kvinnan,'),
 ('Lev',20,13):('annan man','annan man,'),
 ('Lev',20,17):('dotter,','dotter'),
 ('Lev',20,21):('gärning;','gärning:'),
 ('Lev',20,22):('stadgar och göra efter dem, för att landet icke må utspy eder, det land','göra efter dem, för att landet icke må utspy eder, det land,'),
 ('Lev',20,24):('eder I skolen besitta deras land, ty jag skall giva eder det till besittning, ett land','eder: I skolen besitta deras land, ty jag skall giva eder det till besittning, ett land,'),
 ('Lev',20,25):('orena, och mellan rena fåglar och orena, så att I icke gören eder själva till styggelse för de fyrfotadjurs eller fåglars skull eller för de kräldjurs,','orena och mellan rena fåglar och orena, så att I icke gören eder själva till styggelse för de fyrfotadjurs eller fåglars skull eller för de kräldjurs'),
 # Leviticus 21 (EFS 1927 GT s. 169-170 and EFS 1926 GT s. 155-155): L193-L196, readings both prints agree on.
 ('Lev',21,2):('dotter, sin broder; s','s'),
 ('Lev',21,6):('Guds och må icke ohelga sin Guds namn, ty de bära fram HERRENS eldsoffer sin Guds spis; därför skola de','Gud och må icke ohelga sin Guds namn, ty de bära fram HERRENS eldsoffer, sin Guds spis; därför skola de vara'),
 ('Lev',21,7):('en kvinna','en kvinna,'),
 ('Lev',21,10):('utgjuten, ','utgjuten '),
 ('Lev',21,13):('kvinna','kvinna,'),
 ('Lev',21,18):('halt,','halt'),
 ('Lev',21,20):('ögat, eller som har skabb eller annat utslag,','ögat eller som har skabb eller annat utslag'),
 # Leviticus 22 (EFS 1927 GT s. 170-172 and EFS 1926 GT s. 156-157): L197-L208, readings both prints agree on.
 ('Lev',22,2):('säg','säg,'),
 ('Lev',22,3):('min åsyn Jag','min åsyn. Jag'),
 ('Lev',22,5):('smådjur','smådjur,'),
 ('Lev',22,6):('aftonen,','aftonen'),
 ('Lev',22,11):('den träl','den träl,'),
 ('Lev',22,13):('livsfrukt,','livsfrukt'),
 ('Lev',22,15):('barn göra såsom','barn giva såsom'),
 ('Lev',22,18):('offer som de vilja offra åt HERREN såsom brännoffer, så skolen I göra det på sådant sätt','offer, som de vilja offra åt HERREN såsom brännoffer, så skolen I göra det på sådant sätt,'),
 ('Lev',22,20):('djur','djur,'),
 ('Lev',22,21):('felfritt far att','felfritt för att'),
 ('Lev',22,22):('sårigt, eller det som har skabb eller annat utslag','sårigt eller det som har skabb eller annat utslag,'),
 ('Lev',22,29):('sätt','sätt,'),
 # Leviticus 23 (EFS 1927 GT s. 172-174 and EFS 1926 GT s. 157-159): L209-L217, readings both prints agree on.
 ('Lev',23,4):('sammankomster','sammankomster,'),
 ('Lev',23,10):('kärve','kärve,'),
 ('Lev',23,12):('dag då I låten vifta kärven','dag, då I låten vifta kärven,'),
 ('Lev',23,15):('den dag','den dag,'),
 ('Lev',23,17):('orter där I bon','orter, där I bon,'),
 ('Lev',23,20):('ansikte, jämte förstlingsbröden som bäras fram tillika med de båda lammen','ansikte jämte förstlingsbröden som bäras fram tillika med de båda lammen;'),
 ('Lev',23,22):('skörd,','skörd;'),
 ('Lev',23,31):('göra ','göra. '),
 ('Lev',23,38):('löftesoffer,','löftesoffer'),
 ('Lev',23,43):('veta huru jag lät Israels barn bo i lövhyddor, när jag förde dem ut ur Egyptens land Jag','veta, huru jag lät Israels barn bo i lövhyddor, när jag förde dem ut ur Egyptens land. Jag'),
 # Leviticus 24 (EFS 1927 GT s. 174-175 and EFS 1926 GT s. 159-160): L218-L223, readings both prints agree on.
 ('Lev',24,8):('barn,','barn'),
 ('Lev',24,10):('en man','en man,'),
 ('Lev',24,16):('inföding','inföding,'),
 ('Lev',24,19):('vållar att hans nästa får ett lyte, så skall man göra mot honom','vållar, att hans nästa får ett lyte, så skall man göra mot honom,'),
 ('Lev',24,20):('vållat att en annan fick','vållat, att en annan fick,'),
 ('Lev',24,23):('gjorde Israels barn','gjorde Israels barn,'),
 # Leviticus 25 (EFS 1927 GT s. 175-178 and EFS 1926 GT s. 160-163): L224-L235, readings both prints agree on.
 ('Lev',25,3):('åker,','åker'),
 ('Lev',25,5):(' som växa på dina oskurna vinträd',', som växa på dina oskurna vinträd,'),
 ('Lev',25,16):('flera skall du betala högre pris, och alltefter som åren äro färre','flera, skall du betala högre pris, och alltefter som åren äro färre,'),
 ('Lev',25,17):('orätt','orätt,'),
 ('Lev',25,21):('veta','veta,'),
 ('Lev',25,27):('man','man,'),
 ('Lev',25,28):('anskaffa ','anskaffa, '),
 ('Lev',25,29):('hava rätt att återbörda det','hava rätt att återbörda det,'),
 ('Lev',25,32):('i städerna','i städerna.'),
 ('Lev',25,33):('stad ','stad, '),
 ('Lev',25,38):('land,','land'),
 ('Lev',25,42):('säljas','säljas,'),
 ('Lev',25,50):('år då han sålde sig åt honom till jubelåret; och det pris för vilket han såldes','år, då han sålde sig åt honom, till jubelåret; och det pris, för vilket han såldes,'),
 ('Lev',25,53):(' som är lejd för år skall man behandla honom',', som är lejd för år, skall man behandla honom;'),
 # Leviticus 26 (EFS 1927 GT s. 178-180 and EFS 1926 GT s. 163-165): L236-L237, readings both prints agree on.
 ('Lev',26,1):('av g','avg'),
 ('Lev',26,9):(' ö','ö'),
 ('Lev',26,15):('stadgar,','stadgar'),
 ('Lev',26,40):('begått, och den deras fäder hava begått,','begått och den deras fäder hava begått'),
 ('Lev',26,46):('lagar ','lagar, '),
 # Leviticus 27 (EFS 1927 GT s. 180-182 and EFS 1926 GT s. 165-166): L238-L246, readings both prints agree on.
 ('Lev',27,2):('sådant','sådant,'),
 ('Lev',27,8):('armod','armod,'),
 ('Lev',27,9):('de lag man','de slag man'),
 ('Lev',27,16):('arvsbesittning ','arvsbesittning, '),
 ('Lev',27,22):('åker','åker,'),
 ('Lev',27,24):('den av vilken den har blivit köpt,','den, av vilken den har blivit köpt'),
 ('Lev',27,26):(', och som tillhör HERREN redan såsom förstfött, det skall ingen helga; vare sig det är ett djur av fäkreaturen eller ett djur av småboskapen, tillhör det redan HERREN',' och som tillhör HERREN redan såsom förstfött, det skall ingen helga; vare sig det är ett djur av fäkreaturen eller ett djur av småboskapen, tillhör det redan HERREN.'),
 ('Lev',27,28):('åker ','åker, '),
 ('Lev',27,29):('människa som har blivit tillspillogiven','människa, som har blivit tillspillogiven,'),
 ('Lev',27,33):('efterforska','efterforska,'),
 ('Lev',27,34):('bud','bud,')}

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
