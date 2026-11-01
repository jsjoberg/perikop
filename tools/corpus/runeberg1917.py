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
# Verses the e-text joins but EFS 1926 and 1927 print separately: (code, chapter, verse) -> opening of the next verse.
# Applied after REPLACE; later verses in the chapter move up by one.
SPLIT={('Ps',49,19):'så skall dock'}
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
 ('Ps',102,27):('skall förvanda dem','skall förvandla dem,'),
 ('Prov',12,22):('lögnaktiga låppar','lögnaktiga läppar'),
 ('Hag',1,1):('profeten Haggai Serubbabel','profeten Haggai till Serubbabel'),
 ('Hag',1,6):('han far den allenast','han får den allenast'),
 ('Hag',1,14):('HERREN Sebaots; sin Guds','HERREN Sebaots, sin Guds'),
 ('Hag',2,2):('profeten Haggai han sade','profeten Haggai; han sade'),
 ('Hag',2,16):('till HERRENS tempel','till HERRENS tempel;'),
 # 1
 ('Ps',1,1):('man','man,'),
 ('Ps',1,4):('agnar','agnar,'),
 # 4
 ('Ps',4,4):('dock','dock,'),
 # 5
 ('Ps',5,5):('Gud','Gud,'),
 ('Ps',5,11):('bort dem','dem bort'),
 # 7
 ('Ps',7,3):(', såsom lejon,',' såsom lejon'),
 ('Ps',7,5):('ned','den'),
 ('Ps',7,12):(' Gud',' Gud,'),
 ('Ps',7,16):('grav','grav,'),
 # 9
 ('Ps',9,14):('se','se,'),
 ('Ps',9,16):(' som de grävde; i det nät som de lade ut',', som de grävde; i det nät, som de lade ut,'),
 ('Ps',9,17):('Higgajón.','Higgajón,'),
 ('Ps',9,21):('förnimma','förnimma,'),
 # 10
 ('Ps',10,2):('ränker','ränker,'),
 ('Ps',10,13):('hjärta','hjärta,'),
 # 11
 ('Ps',11,1):('HERREN H','HERREN h'),
 ('Ps',11,2):('strängen,','strängen'),
 ('Ps',11,6):('kalk','kalk,'),
 # 12
 ('Ps',12,4):('tunga','tunga,'),
 ('Ps',12,7):('silver','silver,'),
 # 13
 ('Ps',13,2):(';',','),
 ('Ps',13,5):(' på ',' på det '),
 # 14
 ('Ps',14,1):('gör','gör,'),
 ('Ps',14,3):('gör','gör,'),
 # 15
 ('Ps',15,2):(' är',' är,'),
 # 16
 ('Ps',16,3):(' som finnas i landet, de äro de härliga',', som finnas i landet, de äro de härliga,'),
 # 17
 ('Ps',17,3):('du ut','du utforskar det om natten, du '),
 ('Ps',17,12):('ett lejon','ett lejon,'),
 # 18
 ('Ps',18,12):(' som omslöt honom;',', som omslöt honom:'),
 ('Ps',18,44):(' som jag ej kände',', som jag ej kände,'),
 # 19
 ('Ps',19,4):('språk','språk,'),
 ('Ps',19,6):('brudgum','brudgum,'),
 ('Ps',19,13):('själv','själv,'),
 # 20
 ('Ps',20,3):(',',''),
 ('Ps',20,5):(' vad ditt hjärta begär',', vad ditt hjärta begär,'),
 ('Ps',20,7):('jag','jag,'),
 # 21
 ('Ps',21,14):('HERREN','HERRE'),
 # 22
 ('Ps',22,7):('mask,','mask'),
 ('Ps',22,10):('du','du,'),
 ('Ps',22,15):('vatten','vatten,'),
 ('Ps',22,32):(' som skola födas',', som skola födas,'),
 # 23
 ('Ps',23,2):('vatten','vatten,'),
 # 24
 ('Ps',24,6):('släkte','släkte,'),
 # 25
 ('Ps',25,3):('skam kommer','skam komma'),
 ('Ps',25,12):('man','man,'),
 # 26
 ('Ps',26,8):('plats','plats,'),
 # 27
 ('Ps',27,12):('män','män,'),
 ('Ps',27,13):('förvisso','förvisso,'),
 # 28
 ('Ps',28,4):('vedergäll dem','vedergäll dem,'),
 ('Ps',28,5):('händer','händers'),
 # 29
 ('Ps',29,1):('sönd','sön'),
 # 30
 ('Ps',30,10):('blod,','blod'),
 # 31
 ('Ps',31,3):('bort','borg'),
 ('Ps',31,4):('bort','borg'),
 ('Ps',31,5):('nät','nät,'),
 ('Ps',31,21):('tungor','tungorna'),
 ('Ps',31,24):('öve','öva'),
 # 32
 ('Ps',32,1):('den','den,'),
 ('Ps',32,2):('människa','människa,'),
 ('Ps',32,6):('tid','tid,'),
 # 33
 ('Ps',33,12):('är det folk','är det folk,'),
 # 34
 ('Ps',34,1):(', h',', n'),
 ('Ps',34,9):(' att HERREN är god; säll är den',', att HERREN är god; säll är den man,'),
 ('Ps',34,13):('man','man,'),
 # 35
 ('Ps',35,3):(',',''),
 ('Ps',35,20):('frid','frid,'),
 # 36
 ('Ps',36,2):(' vad synden säger till den ogudaktige, till den',', vad synden säger till den ogudaktige, till den,'),
 ('Ps',36,3):('honom','honom,'),
 ('Ps',36,4):('göra','göra,'),
 ('Ps',36,5):('väg','väg,'),
 ('Ps',36,6):('nåd,','nåd'),
 # 37
 ('Ps',37,4):('dig','dig,'),
 ('Ps',37,7):(' vilkens väg är lyckosam, över den man',', vilkens väg är lyckosam, över den man,'),
 ('Ps',37,13):('ser','ser,'),
 ('Ps',37,14):('båge,','båge'),
 ('Ps',37,16):(' som en rättfärdig har',', som en rättfärdig har,'),
 ('Ps',37,26):('låna','låna,'),
 ('Ps',37,28):(' vad rätt är',', vad rätt är,'),
 ('Ps',37,30):('säger','säger,'),
 ('Ps',37,34):('lust','lust,'),
 ('Ps',37,35):('ogudaktig','ogudaktig,'),
 # 38
 ('Ps',38,15):('man','man,'),
 ('Ps',38,17):('fruktar','fruktar,'),
 # 39
 ('Ps',39,3):('ma','me'),
 ('Ps',39,5):(' att jag måste få en ände, och vad som är mina dagars mått, så att jag förstår',', att jag måste få en ände, och vad som är mina dagars mått, så att jag förstår,'),
 ('Ps',39,7):('icke','icke,'),
 ('Ps',39,8):('Herre','HERRE'),
 # 40
 ('Ps',40,4):('häpna,','häpna'),
 ('Ps',40,5):('man','man,'),
 ('Ps',40,8):('skrivet','skrivet,'),
 # 41
 ('Ps',41,5):('HERRE;','HERRE,'),
 ('Ps',41,7):('honom','honom,'),
 # 42
 ('Ps',42,5):('minne','minne,'),
 ('Ps',42,11):('Det är','Det är,'),
 # 43
 ('Ps',43,2):('Gud','Gud,'),
 # 44
 ('Ps',44,3):('var du','var du,'),
 ('Ps',44,12):('han','har'),
 # 45
 ('Ps',45,3):(' vi',' vi,'),
 # 46
 ('Ps',46,9):(' som väcka häpnad',', som väcka häpnad,'),
 ('Ps',46,11):('besinnen','besinnen,'),
 ('Ps',46,12):('men','med'),
 # 47
 ('Ps',47,6):('HERREN,','HERREN'),
 # 49
 ('Ps',49,4):('H','M'),
 ('Ps',49,11):('se','se,'),
 ('Ps',49,12):('tänka','tänka,'),
 ('Ps',49,19):('gör ','gör dig '),
 ('Ps',49,20):('människa','människa,'),
 # 50
 ('Ps',50,6):('förkunna','förkunna,'),
 ('Ps',50,21):('tror du','tror du,'),
 # 51
 ('Ps',51,4):(', och rena mig frå',' och rena mig från mi'),
 ('Ps',51,6):('gjort','gjort,'),
 ('Ps',51,10):(' som du har krossat',', som du har krossat,'),
 ('Ps',51,19):(' som behagar Gud',', som behagar Gud,'),
 # 52
 ('Ps',52,9):('man','man,'),
 ('Ps',52,11):('dig','dig,'),
 # 53
 ('Ps',53,3):(', för att se',' för att se,'),
 ('Ps',53,7):('Ack','Ack,'),
 # 55
 ('Ps',55,13):(' som smädar mig, det kunde jag fördraga; det är icke min ovän',', som smädar mig, det kunde jag fördraga; det är icke min ovän,'),
 ('Ps',55,23):('tillstädja','tillstädja,'),
 # 56
 ('Ps',56,1):('nå','nä'),
 ('Ps',56,10):('dag','dag,'),
 # 57
 ('Ps',57,3):('ropar till Gud','ropar till Gud,'),
 ('Ps',57,4):('människor','människor,'),
 ('Ps',57,5):('människor','människor,'),
 # 58
 ('Ps',58,2):(' vad rättfärdigt är? Dömen I',', vad rättfärdigt är? Dömen I,'),
 ('Ps',58,12):('Gud','Gud,'),
 # 59
 ('Ps',59,13):('lögn','lögn,'),
 ('Ps',59,14):(' att det är Gud',', att det är Gud,'),
 # 62
 ('Ps',62,11):(':',';'),
 # 63
 ('Ps',63,2):('längtar efter dig,','längtar efter dig'),
 # 64
 ('Ps',64,10):(' vad Gud har gjort',', vad Gud har gjort,'),
 # 66
 ('Ps',66,5):('sen','sen,'),
 ('Ps',66,10):(' de',' du'),
 # 68
 ('Ps',68,9):(' för Guds ansikte;',' för Guds ansikte,'),
 ('Ps',68,12):('kvinnor','kvinnor,'),
 ('Ps',68,17):('det berg','det berg,'),
 ('Ps',68,21):(' Gud',' Gud,'),
 ('Ps',68,26):('kvinnor','kvinnor,'),
 ('Ps',68,27):('Lova','Loven'),
 ('Ps',68,31):('de folk','de folk,'),
 # 69
 ('Ps',69,22):(', och ättika att dricka,',' och ättika att dricka'),
 # 71
 ('Ps',71,3):('klippa','klippa,'),
 ('Ps',71,20):('igen du','igen ur'),
 # 72
 ('Ps',72,5):('varar,','varar'),
 ('Ps',72,6):(' som faller på ängen, lik en regnskur',', som faller på ängen, lik en regnskur,'),
 ('Ps',72,12):(' som ropar',', som ropar,'),
 # 73
 ('Ps',73,3):('såg','såg,'),
 ('Ps',73,6):('klädnad','klädnad,'),
 ('Ps',73,17):('på des','på dera'),
 # 74
 ('Ps',74,2):('förlossade,','förlossade'),
 ('Ps',74,3):('plats','plats,'),
 ('Ps',74,10):(', och',', o'),
 ('Ps',74,13):(' som delade havet genom din makt; du krossade drakarnas huvuden mot',', som delade havet genom din makt; du krossade drakarnas huvuden på'),
 ('Ps',74,14):('du','du,'),
 ('Ps',74,15):('var du','var du,'),
 ('Ps',74,16):('natten,','natten;'),
 ('Ps',74,17):('du','du,'),
 ('Ps',74,22):('Betänk','Betänk,'),
 # 75
 ('Ps',75,9):('hand,','hand'),
 # 76
 ('Ps',76,11):('dit','dig'),
 # 77
 ('Ps',77,6):('år','år,'),
 ('Ps',77,15):('en Gud','en Gud,'),
 ('Ps',77,17):(', och',', o'),
 # 78
 ('Ps',78,6):('för barn','för barn,'),
 ('Ps',78,8):('ett släkte','ett släkte,'),
 ('Ps',78,14):(',',''),
 ('Ps',78,21):('jag','ja'),
 ('Ps',78,29):('få','få,'),
 ('Ps',78,39):('vind','vind,'),
 ('Ps',78,42):('icke','icke,'),
 ('Ps',78,54):('berg','berg,'),
 ('Ps',78,57):('båge','båge,'),
 ('Ps',78,65):('hjälte','hjälte,'),
 # 79
 ('Ps',79,6):('riken','riken,'),
 ('Ps',79,10):('på hedningarna','på hedningarna,'),
 ('Ps',79,12):('smädelse','smädelse,'),
 # 80
 ('Ps',80,16):(' som din högra hand har planterat, och den son',', som din högra hand har planterat, och den son,'),
 ('Ps',80,18):('människoson','människoson,'),
 # 81
 ('Ps',81,6):('tal','tal,'),
 ('Ps',81,13):('det','de'),
 # 82
 ('Ps',82,6):('sagt','sagt,'),
 # 83
 ('Ps',83,6):('dem','de'),
 ('Ps',83,10):('dem','dem,'),
 ('Ps',83,12):(' såsom det gick Oreb och Seeb, och alla deras furstar',', såsom det gick Oreb och Seeb, och alla deras furstar,'),
 ('Ps',83,15):(' som förbränner skog och lik en låga som avsvedjar berg',', som förbränner skog, och lik en låga, som avsvedjar berg,'),
 ('Ps',83,19):('förnimma','förnimma,'),
 # 84
 ('Ps',84,6):('människor','människor,'),
 ('Ps',84,10):('ser härtill,','se härtill'),
 ('Ps',84,13):('människa','människa,'),
 # 85
 ('Ps',85,9):('höra','höra,'),
 ('Ps',85,13):('oss','oss,'),
 # 86
 ('Ps',86,10):('stora ',''),
 ('Ps',86,11):('ena','ena,'),
 ('Ps',86,17):('blygd','blygd,'),
 # 88
 ('Ps',88,6):('slagna','slagna,'),
 ('Ps',88,13):(',',''),
 # 89
 ('Ps',89,2):(',',''),
 ('Ps',89,16):(' som vet',', som vet,'),
 ('Ps',89,20):('hans','hand'),
 ('Ps',89,21):('helig','heliga'),
 ('Ps',89,30):('och ',''),
 ('Ps',89,49):(' som får leva och undgår att se döden? Vem räddar d',', som får leva och undgår att se döden? Vem räddar s'),
 ('Ps',89,50):(' forna nådegärningar, vad du lovade David med ed i din trofasthet.','a forna nådegärningar, vad du lovade David med ed i din trofasthet?'),
 # 90
 ('Ps',90,4):('dag','dag,'),
 ('Ps',90,5):('gräset','gräset,'),
 ('Ps',90,12):('betänka','betänka,'),
 ('Ps',90,15):(' som du har plågat oss, så många år',', som du har plågat oss, så många år,'),
 # 92
 ('Ps',92,3):(' när natten har kommit',', när natten har kommit,'),
 ('Ps',92,8):('till ','till deras '),
 ('Ps',92,12):('onda','onda,'),
 ('Ps',92,16):('så för att de skola förkunna','för att de skola förkunna,'),
 # 93
 ('Ps',93,1):('min','nu'),
 # 94
 ('Ps',94,2):('högmodiga','högmodiga,'),
 ('Ps',94,8):('oförnuftiga','oförnuftige'),
 ('Ps',94,11):('vet','vet,'),
 ('Ps',94,12):('man','man,'),
 ('Ps',94,16):('till','för'),
 ('Ps',94,18):('når, o HERRE:','nåd, o HERRE.'),
 ('Ps',94,20):(' där man öve',', där man öva'),
 # 95
 ('Ps',95,7):(' som han har till sin hjord, vi äro får',', som han har till sin hjord, vi äro får,'),
 ('Ps',95,10):('folk','folk,'),
 # 96
 ('Ps',96,12):('.',''),
 # 98
 ('Ps',98,1):(' ära en ny sång, ty han har gjort under. Han har vunnit seger med sin högra hand och med sin väld','S ära en ny sång, ty han har gjort under. Han har vunnit seger med sin högra hand och med sin hel'),
 ('Ps',98,3):('sett','sett,'),
 # 99
 ('Ps',99,4):('älskar','älskar,'),
 ('Ps',99,7):('lag','lag,'),
 # 102
 ('Ps',102,14):('tid','tid,'),
 ('Ps',102,19):(' som varder skapat',', som varder skapat,'),
 ('Ps',102,20):('jorden,','jorden'),
 ('Ps',102,26):('händer','händers'),
 # 103
 ('Ps',103,2):('icke','icke,'),
 ('Ps',103,12):('väster','väster,'),
 ('Ps',103,14):('vet','vet,'),
 # 104
 ('Ps',104,1):('stor,','stor;'),
 ('Ps',104,3):('fram ',''),
 ('Ps',104,8):(', på den plats',' på den plats,'),
 ('Ps',104,18):('är','äro'),
 ('Ps',104,19):('stunden','stunden,'),
 ('Ps',104,25):('stora ock','stora och'),
 # 105
 ('Ps',105,5):('verk','verk,'),
 # 106
 ('Ps',106,15):('han dem','han dem,'),
 ('Ps',106,34):('folk','folk,'),
 ('Ps',106,38):('döttrars blod','döttrars blod,'),
 # 107
 ('Ps',107,4):('stad','stad,'),
 ('Ps',107,7):('stad','stad,'),
 ('Ps',107,30):(' att det vart stilla, och han förde dem till den hamn',', att det vart stilla, och han förde dem till den hamn,'),
 ('Ps',107,32):('lova honom','lova honom,'),
 ('Ps',107,36):('stad','stad,'),
 # 108
 ('Ps',108,4):('HERREN','HERRE'),
 ('Ps',108,5):(',',''),
 # 109
 ('Ps',109,19):(',',''),
 ('Ps',109,21):('namn','namns'),
 ('Ps',109,27):('förnimma','förnimma,'),
 # 111
 ('Ps',111,3):(' är',' är,'),
 # 112
 ('Ps',112,1):('man','man,'),
 # 115
 ('Ps',115,14):(', s',', '),
 # 116
 ('Ps',116,4):('Ack','Ack,'),
 # 118
 ('Ps',118,16):('HERRENS högra han ','HERRENS högra hand '),
 ('Ps',118,21):('dig','dig,'),
 ('Ps',118,22):(' som byggningsmännen förkastade',', som byggningsmännen förkastade,'),
 ('Ps',118,24):('dag','dag,'),
 ('Ps',118,25):(' HERRE, fräls! Ack',', HERRE, fräls! Ack,'),
 ('Ps',118,26):('han','han,'),
 # 119
 ('Ps',119,4):('givit ','givit dina '),
 ('Ps',119,39):('smälek','smälek,'),
 ('Ps',119,42):('S','s'),
 ('Ps',119,50):('lidande','lidande,'),
 ('Ps',119,52):('tröstar','tröstad'),
 ('Ps',119,54):('hus','hus,'),
 ('Ps',119,71):('gott','gott,'),
 ('Ps',119,75):('vet','vet,'),
 ('Ps',119,79):('och de ','och de s'),
 ('Ps',119,127):(', jag',', ja'),
 ('Ps',119,152):('vittnesbörd','vittnesbörd,'),
 # 124
 ('Ps',124,6):('HERREN','HERREN,'),
 # 127
 ('Ps',127,2):(' att I bittida stån upp och sent gån till vila, och äten eder',', att I bittida stån upp och sent gån till vila och äten edert'),
 ('Ps',127,4):('söner','söner,'),
 ('Ps',127,5):('man','man,'),
 # 128
 ('Ps',128,3):('hustru,','hustru'),
 # 129
 ('Ps',129,3):('upp ',''),
 # 131
 ('Ps',131,1):('med ting','med ting,'),
 # 132
 ('Ps',132,2):(';',':'),
 ('Ps',132,3):('hydda','hydda,'),
 # 133
 ('Ps',133,1):('är','är,'),
 # 135
 ('Ps',135,5):('vet','vet,'),
 # 136
 ('Ps',136,3):('HERRE','Herre'),
 ('Ps',136,16):('evinnerligen,','evinnerligen;'),
 # 137
 ('Ps',137,2):(' som där voro',', som där voro,'),
 # 139
 ('Ps',139,14):('dig','dig,'),
 # 140
 ('Ps',140,2):('mig från','mig för'),
 ('Ps',140,9):('HERRE;','HERRE,'),
 ('Ps',140,13):('vet','vet,'),
 # 141
 ('Ps',141,4):('män','män,'),
 ('Ps',141,6):('höra','höra,'),
 ('Ps',141,9):(' som de lägga ut på min väg',', som de lägga ut på min väg,'),
 # 142
 ('Ps',142,4):(' där jag skall gå',', där jag skall gå,'),
 # 143
 ('Ps',143,8):('väg','väg,'),
 # 144
 ('Ps',144,15):('saligt är det folk s','saligt är det folk, s'),
 # 145
 ('Ps',145,19):(' vad de gudfruktiga begära',', vad de gudfruktiga begära,'),
 ('Ps',145,21):('HERREN','HERRENS'),
 # 146
 ('Ps',146,4):('jord','jord,'),
 # 147
 ('Ps',147,9):('ungar','ungar,'),
 # 148
 ('Ps',148,14):('det folk','det folk,'),
 # 149
 ('Ps',149,9):('dom','dom,'),
 # 150
 ('Ps',150,2):('härlighet','härlighet.')}

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
    for (code,chapter,number),start in SPLIT.items():
        text=verses[(code,chapter,number)]
        if text.count(start)!=1:raise ValueError('1917 verse split does not apply once: %s %d:%d'%(code,chapter,number))
        head,tail=text.split(start)
        shifted={}
        for key,value in verses.items():
            if key[:2]==(code,chapter) and key[2]>=number:
                if key[2]==number:shifted[key]=head.rstrip();shifted[(code,chapter,number+1)]=start+tail
                else:shifted[(code,chapter,key[2]+1)]=value
            else:shifted[key]=value
        verses=shifted
    return verses
