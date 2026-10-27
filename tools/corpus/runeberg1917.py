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
RENUMBER={('25',1,21,2):22,('42',17,36,1):37,('42',20,23,1):22}
# E-text errors checked visually against the 1927 facsimile.
# Page links, context and pending human review: docs/reports/sv1917-errata.md.
REPLACE={
 # Genesis 1-3 scan pass: R425; see sv1917-ot.md for retained print variants.
 ('Gen',1,7):('fästet från vattnet ovan fästet','fästet ifrån vattnet ovan fästet'),
 # Genesis 5-8 scan pass: restore an omitted word supported by both prints.
 ('Gen',5,13):('sedan Kenan fött Mahalalel','sedan Kenan hade fött Mahalalel'),
 # Genesis 9-13: both EFS prints support these readings.
 ('Gen',9,5):('den andres själ','den andras själ'),
 ('Gen',15,12):('och ett stort mörker','och stort mörker'),
 ('Gen',15,13):('så skall ske i fyra hundra år.»','så skall ske i fyra hundra år.'),
 ('Gen',13,14):('sedan Lot hade skilt sig från honom: »Lyft upp dina ögon och se, från den plats där du står, mot norr och söder och öster och väster.»','sedan Lot hade skilt sig ifrån honom: »Lyft upp dina ögon och se, från den plats där du står, mot norr och söder och öster och väster.'),
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
 # Matthew 1-10 scan pass and supplementary photos: R024-R030.
 ('Matt', 1, 19):('utsätta henne for vanära', 'utsätta henne för vanära'),
 ('Matt', 2, 19):('en Herrens ängel for Josef', 'en Herrens ängel för Josef'),
 ('Matt', 9, 17):('nytt vin I gamla', 'nytt vin i gamla'),
 ('Matt', 10, 27):('det skolen säga i ljuset', 'det skolen I säga i ljuset'),
 ('Matt', 3, 15):('all rättfärdighet. Då', 'all rättfärdighet.» Då'),
 ('Matt', 1, 24):('gjorde han som Herrens ängel', 'gjorde han såsom Herrens ängel'),
 ('Matt', 10, 11):('och stannen hos honom', 'och stannen så hos honom'),
 # Matthew 11-18 scan pass: R031-R035; printing questions are deferred in photo batch 2.
 ('Matt', 13, 15):('helade av mig.', "helade av mig.'"),
 ('Matt', 13, 27):('varifrån har den då fått ogräs?', "varifrån har den då fått ogräs?'"),
 ('Matt', 14, 20):('blevo mätta Sedan', 'blevo mätta. Sedan'),
 ('Matt', 17, 27):('åt dem for mig och dig', 'åt dem för mig och dig'),
 ('Matt', 18, 5):('ett sådant barn I mitt namn', 'ett sådant barn i mitt namn'),
 # Matthew 19-28 scan pass: R036-R041; printing questions are saved separately.
 ('Matt', 20, 11):('knorrade de mot husbonden.', 'knorrade de mot husbonden'),
 ('Matt', 21, 24):('myndighet jag gör detta».', 'myndighet jag gör detta.'),
 ('Matt', 21, 42):('byggningsmännen förkastade. den', 'byggningsmännen förkastade, den'),
 ('Matt', 23, 23):('skriftlärde och fariséer. I skrymtare', 'skriftlärde och fariséer, I skrymtare'),
 ('Matt', 25, 46):('dessa skola då då bort', 'dessa skola då gå bort'),
 ('Matt', 27, 47):('som stodo där borde detta', 'som stodo där hörde detta'),
 # Mark 1-16 scan pass: R042-R051; unresolved readings are saved in photo batch 4.
 ('Mark', 2, 28):('Så år då Människosonen', 'Så är då Människosonen'),
 ('Mark', 3, 11):('för honom och och ropade', 'för honom och ropade'),
 ('Mark', 3, 33):('och sade: Vilken är min moder', 'och sade: »Vilken är min moder'),
 ('Mark', 6, 16):('Han bar uppstått från de döda', 'Han har uppstått från de döda'),
 ('Mark', 6, 19):('Därför hyste nu Herodes agg till honom och ville döda honom, men han hade icke makt därtill.', 'Därför hyste nu Herodias agg till honom och ville döda honom, men hon hade icke makt därtill.'),
 ('Mark', 10, 19):("tillkommer', Hedra din fader", "tillkommer', 'Hedra din fader"),
 ('Mark', 11, 3):('sända den tillbaka hit.»', "sända den tillbaka hit.'»"),
 ('Mark', 10, 12):('då begår hon äktenskapsbrott.', 'då begår hon äktenskapsbrott.»'),
 ('Mark', 13, 34):('bjuder portvaktaren att vaka.', 'bjuder portvaktaren att vaka,'),
 ('Mark', 16, 7):('såsom han bar sagt eder', 'såsom han har sagt eder'),

 # Luke 1-8 scan pass: R052-R074; printing questions are saved in photo batch 5.
 ('Luke', 1, 70):('sin forntida heliga profeters mun', 'sina forntida heliga profeters mun'),
 ('Luke', 1, 73):('vad han med ed hade lovat för vår fader Abraham,', 'vad han med ed hade lovat vår fader Abraham.'),
 ('Luke', 3, 4):('i öknen: Bereden vägen', "i öknen: 'Bereden vägen"),
 ('Luke', 3, 9):('Redan är också yxan satt till roten på träden; så bliver då vart träd som icke bär god frukt avhugget och kastat på elden.', 'Redan är också yxan satt till roten på träden; så bliver då vart träd som icke bär god frukt avhugget och kastat i elden.»'),
 ('Luke', 3, 14):('Vad skola då vi göra? Han', 'Vad skola då vi göra?» Han'),
 ('Luke', 3, 16):('tog till orda och sade: till dem alla:', 'tog till orda och sade till dem alla:'),
 ('Luke', 3, 22):('över honom I lekamlig skepnad', 'över honom i lekamlig skepnad'),
 ('Luke', 4, 16):('in i synagogan: och där', 'in i synagogan; och där'),
 ('Luke', 4, 25):("på Elias' tid då himmelen", "på Elias' tid – då himmelen"),
 ('Luke', 5, 31):('»De är icke de friska', '»Det är icke de friska'),
 ('Luke', 6, 2):('Då sade några av fariséerna; »Huru kunnen I göra vad som icke är lovligt att göra på sabbaten?', 'Då sade några av fariséerna: »Huru kunnen I göra vad som icke är lovligt att göra på sabbaten?»'),
 ('Luke', 6, 20):('och säg på sina lärjungar', 'och såg på sina lärjungar'),
 ('Luke', 6, 23):('På samma satt gjorde', 'På samma sätt gjorde'),
 ('Luke', 6, 26):('gjorde ju deras fader i fråga', 'gjorde ju deras fäder i fråga'),
 ('Luke', 6, 42):('att du kan tala ut grandet', 'att du kan taga ut grandet'),
 ('Luke', 7, 7):('värdig att komma till dig Men säg', 'värdig att komma till dig. Men säg'),
 ('Luke', 7, 16):('och sade: Den stor profet', 'och sade: »En stor profet'),
 ('Luke', 7, 20):('När mannen kommo fram', 'När männen kommo fram'),
 ('Luke', 7, 40):('»Mästare, säg det.', '»Mästare, säg det.»'),
 ('Luke', 7, 43):('»Rätt dömde du»', '»Rätt dömde du.»'),
 ('Luke', 8, 10):('för att de med seende ögon', "för att de 'med seende ögon"),
 ('Luke', 8, 20):('Din moder och dina broder', 'Din moder och dina bröder'),
 ('Luke', 8, 44):('strax stannade hennes blodgång', 'strax stannade hennes blodgång.'),

 # Luke 9-12 scan pass: R075-R103; unresolved readings are retained in photo batch 6.
 ('Luke', 9, 4):('kommit in något hus', 'kommit in i något hus'),
 ('Luke', 9, 10):('De tog han dem med sig', 'Då tog han dem med sig'),
 ('Luke', 9, 16):('och säg upp till himmelen', 'och såg upp till himmelen'),
 ('Luke', 9, 18):('var stadd i byn', 'var stadd i bön'),
 ('Luke', 9, 26):('Den som blyges för mig och för mina ord, för honom skall Människosonen blygas, när han kom mer i sin och min Faders och de heliga änglarnas härlighet.', 'Den som blyges för mig och för mina ord, för honom skall Människosonen blygas, när han kommer i sin och sin Faders och de heliga änglarnas härlighet.'),
 ('Luke', 9, 30):('Och de, två män stodo', 'Och se, två män stodo'),
 ('Luke', 9, 33):('Mästare, har är oss gott', 'Mästare, här är oss gott'),
 ('Luke', 9, 44):('bliva överlämnad i människors händer.', 'bliva överlämnad i människors händer.»'),
 ('Luke', 9, 48):('han är störst.', 'han är störst.»'),
 ('Luke', 9, 49):('Mästare, sågo huru en man drev ut onda andar genom ditt namn; och du ville hindra honom', 'Mästare, vi sågo huru en man drev ut onda andar genom ditt namn; och vi ville hindra honom'),
 ('Luke', 9, 54):('lärjungarna Jakob i och Johannes', 'lärjungarna Jakob och Johannes'),
 ('Luke', 9, 57):('Jag vill följa dig, varthelst du går.', 'Jag vill följa dig, varthelst du går.»'),
 ('Luke', 9, 59):('Föl; mig.', 'Följ mig.'),
 ('Luke', 10, 1):('dit han själv tänkte komma', 'dit han själv tänkte komma.'),
 ('Luke', 10, 15):('Och du. Kapernaum', 'Och du, Kapernaum'),
 ('Luke', 10, 20):('över att änglarna äro eder underdåniga', 'över att andarna äro eder underdåniga'),
 ('Luke', 10, 22):('blivit för trott åt mig', 'blivit förtrott åt mig'),
 ('Luke', 10, 28):('Gör det, så får du leva,', 'Gör det, så får du leva.»'),
 ('Luke', 11, 7):('stå upp och göra dig något', 'stå upp och giva dig något'),
 ('Luke', 11, 11):('i stallet för en fisk', 'i stället för en fisk'),
 ('Luke', 11, 27):('När han sade detta, hov en kvinna in folkhopen upp sin röst och ropade till honom: »Saligt är det moderssköte som har burit dig, och de bröst som du har diat.»', 'När han sade detta, hov en kvinna i folkhopen upp sin röst och ropade till honom: »Saligt är det moderssköte som har burit dig, och det bröst som du har diat.»'),
 ('Luke', 11, 28):('saliga är de som höra', 'saliga äro de som höra'),
 ('Luke', 11, 29):('ett ont släkte Det begär', 'ett ont släkte. Det begär'),
 ('Luke', 11, 31):('och se, har är vad som är mer än Salomo', 'och se, här är vad som är mer än Salomo'),
 ('Luke', 12, 19):('Sedan vill jag säga till min själ: Kära själ, du har mycket gott förvarat för många år; giv dig nu ro, ät, drick och var glad.', "Sedan vill jag säga till min själ: 'Kära själ, du har mycket gott förvarat för många år; giv dig nu ro, ät, drick och var glad.'"),
 ('Luke', 12, 33):('där man icke fördärvar', 'där mal icke fördärvar'),
 ('Luke', 12, 40):('Så varen ock I redo ty', 'Så varen ock I redo, ty'),
 ('Luke', 12, 54):('Han hade också till folket', 'Han sade också till folket'),
 ('Luke', 12, 53):('sonhustrun mot sin svärmoder.', 'sonhustrun mot sin svärmoder.»'),

 # Luke 13-17 scan pass: R104-R123; wording/numbering questions are saved in photo batch 7.
 ('Luke', 13, 9):('till nästa å bära frukt', 'till nästa år bära frukt'),
 ('Luke', 13, 12):('sade till henne »Kvinna, du är fri ifrån din sjuk dom»,', 'sade till henne: »Kvinna, du är fri ifrån din sjukdom»,'),
 ('Luke', 13, 14):('men icke på sabbatsdagen.', 'men icke på sabbatsdagen.»'),
 ('Luke', 13, 34):('Men haven icke velat.', 'Men I haven icke velat.'),
 ('Luke', 13, 35):("förrän den tid kommen, då I sägen: 'Välsignad vare han som kommer, i Herrens namn.'", "förrän den tid kommer, då I sägen: 'Välsignad vare han som kommer, i Herrens namn.'»"),
 ('Luke', 14, 22):('men har är ännu rum', 'men här är ännu rum'),
 ('Luke', 14, 24):('skall smaka sin måltid', 'skall smaka min måltid'),
 ('Luke', 14, 28):('kostnaden och mer till', 'kostnaden och ser till'),
 ('Luke', 15, 10):('en enda syndare som gör bättring.', 'en enda syndare som gör bättring.»'),
 ('Luke', 15, 14):('kom en svär hungersnöd', 'kom en svår hungersnöd'),
 ('Luke', 15, 17):('medan jag har förgås av hunger', 'medan jag här förgås av hunger'),
 ('Luke', 15, 18):('och säga till honom: Fader,', "och säga till honom: 'Fader,"),
 ('Luke', 15, 22):("Då sade fadern till sina tjänare '", "Då sade fadern till sina tjänare: '"),
 ('Luke', 15, 23):('så vilja vi äta och gör oss glada', 'så vilja vi äta och göra oss glada'),
 ('Luke', 15, 28):('och talade vanligt med honom', 'och talade vänligt med honom'),
 ('Luke', 15, 29):('och lik väl har du åt mig', 'och likväl har du åt mig'),
 ('Luke', 16, 26):('över till oss.»', "över till oss.'"),
 ('Luke', 17, 3):('Om din broder försyndat sig', 'Om din broder försyndar sig'),
 ('Luke', 17, 16):('på mitt ansikte för Jesu fötter', 'på sitt ansikte för Jesu fötter'),
 ('Luke', 17, 24):('på hand dag', 'på hans dag'),

 # Luke 18-24 scan pass: R124-R156; wording questions are saved in photo batch 8.
 ('Luke', 18, 14):('rättfärdig mer an den andre', 'rättfärdig mer än den andre'),
 ('Luke', 18, 30):('som icke skall mångfaldigt igen', 'som icke skall få mångfaldigt igen'),
 ('Luke', 19, 8):('så giver jag fyradubbelt igen.', 'så giver jag fyradubbelt igen.»'),
 ('Luke', 19, 13):('till sig tio sin tjänare', 'till sig tio sina tjänare'),
 ('Luke', 19, 14):('denne skall bliva konung över oss.»', "denne skall bliva konung över oss.'"),
 ('Luke', 19, 27):('fören dem hit huggen ned dem', 'fören dem hit, och huggen ned dem'),
 ('Luke', 20, 2):('har rivit dig sådan myndighet', 'har givit dig sådan myndighet'),
 ('Luke', 20, 9):('och för utrikes för lång tid', 'och for utrikes för lång tid'),
 ('Luke', 20, 14):('Men när vingårdsmannen fingo se honom', 'Men när vingårdsmännen fingo se honom'),
 ('Luke', 20, 28):('hustru till akta', 'hustru till äkta'),
 ('Luke', 20, 35):('varken män tag sig hustrur, eller hustrur givas män', 'varken män taga sig hustrur, eller hustrur givas åt män'),
 ('Luke', 20, 36):('mer dö ty de äro lika änglarna och äro, Guds söner', 'mer dö; ty de äro lika änglarna och äro Guds söner'),
 ('Luke', 20, 42):('Psalmernas bok: Herren sade till min herre:', "Psalmernas bok: 'Herren sade till min herre:"),
 ('Luke', 20, 47):('hålla långa baner. Del skola', 'hålla långa böner. De skola'),
 ('Luke', 21, 25):('i solen och månen och i stjärnorna', 'i solen och i månen och i stjärnorna'),
 ('Luke', 22, 7):('i det osyrad brödets högtid', 'i det osyrade brödets högtid'),
 ('Luke', 22, 9):('De frågade honom: Var vill du', 'De frågade honom: »Var vill du'),
 ('Luke', 22, 10):('Han svarade dem: När I kommen', 'Han svarade dem: »När I kommen'),
 ('Luke', 22, 37):('det går nu i fullbordan»', 'det går nu i fullbordan.»'),
 ('Luke', 22, 53):('händer emot mig men detta', 'händer emot mig; men detta'),
 ('Luke', 22, 56):('där han satt vid elden fäste ögonen på honom och sade: »Också denne var med honom.', 'där han satt vid elden, fäste ögonen på honom och sade: »Också denne var med honom.»'),
 ('Luke', 22, 58):('en av mannen, se honom', 'en av männen, se honom'),
 ('Luke', 23, 3):('och sade: Är du judarnas konung?»', 'och sade: »Är du judarnas konung?»'),
 ('Luke', 23, 7):('under dessa dalar också', 'under dessa dagar också'),
 ('Luke', 23, 22):('som förtjänar döden Därför vill jag', 'som förtjänar döden. Därför vill jag'),
 ('Luke', 23, 30):('säga till: bergen', 'säga till bergen'),
 ('Luke', 23, 34):('Men Jesus sade. »Fader, förlåt dem; ty de veta icke vad de göra.', 'Men Jesus sade: »Fader, förlåt dem; ty de veta icke vad de göra.»'),
 ('Luke', 23, 50):('en rådsherre, vi namn Josef', 'en rådsherre, vid namn Josef'),
 ('Luke', 24, 4):('då stodo två man framför dem', 'då stodo två män framför dem'),
 ('Luke', 24, 5):('Då sade mannen till dem »', 'Då sade männen till dem: »'),
 ('Luke', 24, 6):('Han är icke har, han är uppstånden.', 'Han är icke här, han är uppstånden.'),
 ('Luke', 24, 26):('för I att så ingå', 'för att så ingå'),
 ('Luke', 24, 36):('»Frid vare med eder.', '»Frid vare med eder.»'),

 # John 1-8 scan pass: R157-R177; wording questions are saved in photo batch 9.
 ('John', 1, 40):('hört var Johannes sade', 'hört vad Johannes sade'),
 ('John', 3, 12):('Tron i icke, när jag talar', 'Tron I icke, när jag talar'),
 ('John', 3, 19):('eftersom deras gärningar voro onda,', 'eftersom deras gärningar voro onda.'),
 ('John', 3, 27):('givet från himmelen.»', 'givet från himmelen.'),
 ('John', 4, 17):('att du icke har någon man.»', 'att du icke har någon man.'),
 ('John', 4, 18):('däri sade du sant.', 'däri sade du sant.»'),
 ('John', 4, 34):('och att fullborda hans verk.»', 'och att fullborda hans verk.'),
 ('John', 4, 40):('både de honom att stanna', 'bådo de honom att stanna'),
 ('John', 5, 12):('att du skulle taga sin säng', 'att du skulle taga din säng'),
 ('John', 5, 23):('såsom de ära Faderns.', 'såsom de ära Fadern.'),
 ('John', 5, 25):('Sannerligen säger jag eder: Den stund kommer, ja, den är redan inne, så de döda', 'Sannerligen, sannerligen säger jag eder: Den stund kommer, ja, den är redan inne, då de döda'),
 ('John', 5, 33):('och han har vittnat för sanningen,', 'och han har vittnat för sanningen.'),
 ('John', 6, 14):('Då nu människorna hade det tecken', 'Då nu människorna hade sett det tecken'),
 ('John', 7, 49):('det är förbannat.', 'det är förbannat.»'),
 ('John', 8, 18):('Fadern, som har sänt mig.', 'Fadern, som har sänt mig.»'),
 ('John', 8, 21):('Dig jag går, dit kunnen I', 'Dit jag går, dit kunnen I'),
 ('John', 8, 28):('då skolen I första att', 'då skolen I förstå att'),
 ('John', 8, 36):('så bliven i verkligen fria', 'så bliven I verkligen fria'),
 ('John', 8, 44):('det viljen i göra', 'det viljen I göra'),
 ('John', 8, 47):('som I icke lyssnen därtill.', 'som I icke lyssnen därtill.»'),
 ('John', 8, 54):('han som I säger vara eder Gud', 'han som I sägen vara eder Gud'),

 # John 9-21 scan pass: R178-R197; wording questions are saved in photo batch 10.
 ('John', 9, 8):('den man som att och tiggde', 'den man som satt och tiggde'),
 ('John', 9, 18):('till sig mannen föräldrar', 'till sig mannens föräldrar'),
 ('John', 9, 20):('Då svarade han föräldrar', 'Då svarade hans föräldrar'),
 ('John', 11, 20):('Då nu Maria fick höra att Jesus kom', 'Då nu Marta fick höra att Jesus kom'),
 ('John', 11, 56):('där de stodo i helgedom:', 'där de stodo i helgedomen:'),
 ('John', 12, 30):('utan för eder skull.»', 'utan för eder skull.'),
 ('John', 12, 42):('för fariséernas skulle ville', 'för fariséernas skull ville'),
 ('John', 13, 13):('och I säger rätt', 'och I sägen rätt'),
 ('John', 15, 8):('att i bären mycken frukt', 'att I bären mycken frukt'),
 ('John', 16, 20):('säger jag eder: I skolen bliva bedrövade', 'säger jag eder: I skolen komma att gråta och jämra eder, men världen skall glädja sig; I skolen bliva bedrövade'),
 ('John', 16, 32):('redan kommen, så I skolen förskingras', 'redan kommen, då I skolen förskingras'),
 ('John', 16, 33):('I världen liden i betryck', 'I världen liden I betryck'),
 ('John', 17, 8):('har jag givit åt dem: och de', 'har jag givit åt dem; och de'),
 ('John', 17, 13):('Nu går jag till tid; dock', 'Nu går jag till dig; dock'),
 ('John', 19, 20):('låg nära staden: och den var', 'låg nära staden; och den var'),
 ('John', 19, 29):('Där stod då en kärl', 'Där stod då ett kärl'),
 ('John', 19, 34):('stack upp han sida med ett spjut', 'stack upp hans sida med ett spjut'),
 ('John', 20, 5):('när han lutade sig ditin, så han linnebindlarna', 'när han lutade sig ditin, såg han linnebindlarna'),
 ('John', 20, 26):('och stod mitt ibland de, och sade', 'och stod mitt ibland dem, och sade'),
 ('John', 20, 27):('Räck hit dit finger', 'Räck hit ditt finger'),

 # Acts 1-12 scan pass: R198-R216; wording questions are saved in NT photo batch 1.
 ('Acts', 2, 35):('dig till en fotapall.', "dig till en fotapall.'"),
 ('Acts', 2, 41):('togo emot hans ort', 'togo emot hans ord'),
 ('Acts', 2, 47):('allt folket vad dem', 'allt folket var dem'),
 ('Acts', 3, 21):('de tider nå allt', 'de tider då allt'),
 ('Acts', 4, 7):('Av vilken makt eller i genom', 'Av vilken makt eller genom'),
 ('Acts', 4, 19):('eder mer är Gud', 'eder mer än Gud'),
 ('Acts', 4, 30):('du uträcker din hand', 'du utsträcker din hand'),
 ('Acts', 4, 32):('utan du hade allting', 'utan de hade allting'),
 ('Acts', 5, 24):('visst de icke', 'visste de icke'),
 ('Acts', 5, 25):('berättade för den:', 'berättade för dem:'),
 ('Acts', 7, 4):('efter han faders död', 'efter hans faders död'),
 ('Acts', 7, 13):('fick kunskap och Josefs släkt', 'fick kunskap om Josefs släkt'),
 ('Acts', 7, 30):('när fyrtio är äter voro', 'när fyrtio år åter voro'),
 ('Acts', 7, 34):('sända dig till Egypten.', "sända dig till Egypten.'"),
 ('Acts', 7, 38):('Det var och han,', 'Det var ock han,'),
 ('Acts', 9, 39):('Dorkas hade gjord,', 'Dorkas hade gjort,'),
 ('Acts', 9, 41):('levande framför den.', 'levande framför dem.'),
 ('Acts', 10, 32):('ock kallas Petrus: han', 'ock kallas Petrus; han'),
 ('Acts', 12, 8):('Och han gjorde så. därefter', 'Och han gjorde så. Därefter'),

 # Acts 13-28 scan pass: R217-R248; wording questions are saved in NT photo batch 2.
 ('Acts', 13, 25):('lösa av han fötter', 'lösa av hans fötter'),
 ('Acts', 13, 38):('veta, mena bröder', 'veta, mina bröder'),
 ('Acts', 13, 42):('bad men dem', 'bad man dem'),
 ('Acts', 15, 35):('jämte många andra förkunnade', 'jämte många andra, förkunnade'),
 ('Acts', 16, 17):('Denne följde efter Paulus', 'Denna följde efter Paulus'),
 ('Acts', 16, 20):('fört dem tid fram', 'fört dem dit fram'),
 ('Acts', 17, 7):('Jesus, är konung.', 'Jesus, är konung.»'),
 ('Acts', 17, 16):('Men, Paulus nu väntade', 'Medan Paulus nu väntade'),
 ('Acts', 18, 25):('Denne sade blivit undervisad', 'Denne hade blivit undervisad'),
 ('Acts', 19, 3):("Johannes' döpelse»", "Johannes' döpelse.»"),
 ('Acts', 19, 13):('som Paulus predikar.', 'som Paulus predikar.»'),
 ('Acts', 19, 25):('»I man, I veten', '»I män, I veten'),
 ('Acts', 19, 29):("Paulus' följeslagare", "Paulus' följeslagare."),
 ('Acts', 19, 37):('ej heller smäda vår gudinna,', 'ej heller smäda vår gudinna.'),
 ('Acts', 20, 9):('var han död', 'var han död.'),
 ('Acts', 20, 13):('fara land vägen.', 'fara landvägen.'),
 ('Acts', 20, 18):('umgåtts med eder:', 'umgåtts med eder;'),
 ('Acts', 21, 32):('upphörde de att slå Paulus', 'upphörde de att slå Paulus.'),
 ('Acts', 22, 11):('icke mer kunde se togo', 'icke mer kunde se, togo'),
 ('Acts', 22, 12):('en efter lagen fram man', 'en efter lagen from man'),
 ('Acts', 22, 21):('Då sade han till mig: Gå;', "Då sade han till mig: 'Gå;"),
 ('Acts', 22, 28):('Översten sade då: Mig', 'Översten sade då: »Mig'),
 ('Acts', 22, 30):('av judarna, låt han', 'av judarna, lät han'),
 ('Acts', 25, 24):('som ären har tillstädes', 'som ären här tillstädes'),
 ('Acts', 25, 26):('rannsakning har; blivit', 'rannsakning har blivit'),
 ('Acts', 26, 14):("förföljer du mig'?", 'förföljer du mig?'),
 ('Acts', 26, 24):('på detta satt försvarade', 'på detta sätt försvarade'),
 ('Acts', 27, 10):('»I mån, jag ser', '»I män, jag ser'),
 ('Acts', 27, 34):('ett huvudhår gå förlorat.', 'ett huvudhår gå förlorat.»'),
 ('Acts', 28, 22):('allestädes mötes med gensägelse.', 'allestädes mötes med gensägelse.»'),
 ('Acts', 28, 27):("helade av mig'", "helade av mig.'"),
 ('Acts', 28, 28):('hedningarna bar denna', 'hedningarna har denna'),

 # Romans 1-16 scan pass: R249-R252; wording questions are saved in NT photo batch 3.
 ('Rom', 2, 14):('sig själv en lag', 'sig själva en lag'),
 ('Rom', 8, 13):('Ty om i leven', 'Ty om I leven'),
 ('Rom', 10, 15):('Så är och skrivet:', 'Så är ock skrivet:'),
 ('Rom', 16, 23):('Kvartus hälsar eder.', 'Kvartus hälsa eder.'),

 # 1 Corinthians 1-16 scan pass: R253-R259; wording questions are saved in NT photo batch 4.
 ('1Cor', 9, 20):('för dom som stå under lagen', 'för dem som stå under lagen'),
 ('1Cor', 10, 7):('upp till all leka', 'upp till att leka'),
 ('1Cor', 11, 13):('en kvinnan att ohöljd', 'en kvinna att ohöljd'),
 ('1Cor', 11, 16):('så mån han veta', 'så må han veta'),
 ('1Cor', 11, 22):('äta ock dricka', 'äta och dricka'),
 ('1Cor', 15, 30):('varför undsätta vi oss', 'varför utsätta vi oss'),
 ('1Cor', 16, 19):('Asien hälsar eder.', 'Asien hälsa eder.'),

 # 2 Corinthians 1-13 scan pass: R260-R261; wording questions are saved in NT photo batch 5.
 ('2Cor', 4, 18):('allenast en tid, med de', 'allenast en tid, men de'),
 ('2Cor', 12, 9):('Herren har sagt till mig;', 'Herren har sagt till mig:'),

 # Galatians 1-6 scan pass: R262-R263; wording questions are saved in NT photo batch 6.
 ('Gal', 4, 17):('nej, del vilja avspärra', 'nej, de vilja avspärra'),
 ('Gal', 4, 27):('icke bliver moder, Ty', 'icke bliver moder. Ty'),

 # Philippians through 2 Thessalonians full scan pass: R264-R270; NT photo batch 7.
 ('Col', 2, 17):('skulla komma', 'skulle komma'),
 ('Col', 3, 8):('lägga bort alltsammans; vrede', 'lägga bort alltsammans: vrede'),
 ('1Thess', 1, 4):('bröder, i Guds älskade', 'bröder, I Guds älskade'),
 ('1Thess', 2, 2):('såsom i veten', 'såsom I veten'),
 ('1Thess', 5, 4):('I leven icke mörker', 'I leven icke i mörker'),
 ('1Thess', 5, 20):('profetisk tal', 'profetiskt tal'),
 ('2Thess', 2, 3):('bedraga eder om vad sätt', 'bedraga eder på vad sätt'),

 # 1 Timothy through Philemon full scan pass: R271-R273; NT photo batch 8.
 ('1Tim', 3, 5):('huru skulle dem som icke vet', 'huru skulle den som icke vet'),
 ('1Tim', 4, 4):('mottages med tacksägelse:', 'mottages med tacksägelse;'),
 ('Philemon', 1, 11):('som förut var ingalunda var dig till »till gagn»', 'som förut ingalunda var dig »till gagn»'),

 # Hebrews full scan pass: R274; NT photo batch 9.
 ('Heb', 12, 11):('Väl synes alla aga', 'Väl synes all aga'),

 # James full scan pass: R275-R277; NT photo batch 9.
 ('James', 2, 5):('Har icke Gud utvald just', 'Har icke Gud utvalt just'),
 ('James', 4, 1):('krig i eder lemmar', 'krig i edra lemmar'),
 ('James', 4, 15):('I borden fastmera säga: »Om Herren vill, och vi får leva', 'I borden fastmer säga: »Om Herren vill, och vi få leva'),

 # Peter and John letters full scan pass: R278-R288; NT photo batch 10.
 ('1Peter', 2, 20):('välbehaglig för Gud.', 'välbehagligt för Gud.'),
 ('1Peter', 2, 22):('fanns i hans mun.', 'fanns i hans mun.»'),
 ('1Peter', 3, 1):('Sammalunda, i hustrur, underordnen eder edra män, för att också de män, som till äventyrs icke hörsamma ordet; må genom sina hustrurs vandel bliva vunna utan ord,', 'Sammalunda, I hustrur, underordnen eder edra män, för att också de män, som till äventyrs icke hörsamma ordet, må genom sina hustrurs vandel bliva vunna utan ord,'),
 ('1Peter', 3, 11):('trakte därefter', 'trakte därefter.'),
 ('1Peter', 3, 19):('I anden gick han gick han ock åstad', 'I anden gick han ock åstad'),
 ('1Peter', 3, 21):('Efter denna förebild bliven nu och I frälsta', 'Efter denna förebild bliven nu ock I frälsta'),
 ('1Peter', 4, 6):('så som Gud lever', 'så som Gud lever.'),
 ('1Peter', 4, 11):('tal i enlighet med Guds ord, om någon', 'tal i enlighet med Guds ord; om någon'),
 ('2Peter', 1, 9):('från sina förra synder,', 'från sina förra synder.'),
 ('1John', 3, 5):('Och i veten att Han uppenbarades', 'Och I veten att Han uppenbarades'),
 ('1John', 3, 11):('att vi skola älska varandra', 'att vi skola älska varandra.'),

 # Revelation full scan pass: R289-R320; NT photo batch 11.
 ('Rev', 2, 10):('giva dig livets krona,', 'giva dig livets krona.'),
 ('Rev', 2, 26):('makt över hedningarna.', 'makt över hedningarna,'),
 ('Rev', 3, 3):('och tag vara på därpå', 'och tag vara därpå'),
 ('Rev', 3, 9):('de komma ock falla ned', 'de komma och falla ned'),
 ('Rev', 3, 13):('Anden säger till församlingarna.', 'Anden säger till församlingarna».'),
 ('Rev', 3, 14):('den trovärdiga och sannfärdiga vittnet', 'det trovärdiga och sannfärdiga vittnet'),
 ('Rev', 3, 22):('Anden säger till församlingarna.', 'Anden säger till församlingarna.»'),
 ('Rev', 4, 10):('inför tronen och säga;', 'inför tronen och säga:'),
 ('Rev', 5, 5):('bokrullen och och bryta', 'bokrullen och bryta'),
 ('Rev', 5, 10):('regera på jorden»', 'regera på jorden.»'),
 ('Rev', 7, 4):('av alla Israels barns stammar;', 'av alla Israels barns stammar:'),
 ('Rev', 7, 16):('någon brännande hetta.', "någon brännande hetta.'"),
 ('Rev', 9, 5):('Och åt dem blev givet att, icke att döda dem', 'Och åt dem blev givet, icke att döda dem'),
 ('Rev', 10, 11):('tungomål och konungar.', 'tungomål och konungar.»'),
 ('Rev', 12, 4):('och kastade den ned på jorden', 'och kastade dem ned på jorden'),
 ('Rev', 12, 5):('hon födde ett barn gossebarn', 'hon födde ett gossebarn'),
 ('Rev', 13, 4):('de tillbådo och vilddjuret och sade »', 'de tillbådo ock vilddjuret och sade: »'),
 ('Rev', 13, 6):('Och den öppnade sin mun', 'Och det öppnade sin mun'),
 ('Rev', 14, 7):('himmel och jord och hav och vattenkällor.', 'himmel och jord och hav och vattenkällor.»'),
 ('Rev', 14, 11):('ingen ro, vaken dag eller natt', 'ingen ro, varken dag eller natt'),
 ('Rev', 14, 18):('lien; han sade »', 'lien; han sade: »'),
 ('Rev', 14, 20):('Och vinpressen trampade utanför staden', 'Och vinpressen trampades utanför staden'),
 ('Rev', 16, 17):('Och den sjunde göt ut sin skål i luften. Då gick en stark röst ut från tronen i templet och sade »Det är gjort.»', 'Och den sjunde göt ut sin skål i luften. Då gick en stark röst ut ifrån tronen i templet och sade: »Det är gjort.»'),
 ('Rev', 18, 7):('drottning och och sitter', 'drottning och sitter'),
 ('Rev', 18, 18):('och när de ser röken', 'och när de se röken'),
 ('Rev', 18, 19):('I ett ögonblick har den nu blivit förödd.', "I ett ögonblick har den nu blivit förödd.'"),
 ('Rev', 19, 1):('i himmelen, som sade »', 'i himmelen, som sade: »'),
 ('Rev', 20, 13):('och dessa blev dömda', 'och dessa blevo dömda'),
 ('Rev', 21, 20):('den åttonde den beryll', 'den åttonde en beryll'),
 ('Rev', 21, 22):('Och jag såg i den intet tempel, ty', 'Och jag såg i den intet tempel; ty'),
 ('Rev', 22, 3):('och hans tjänare skall tjäna honom', 'och hans tjänare skola tjäna honom'),
 ('Rev', 22, 17):('han säge »Kom.»', 'han säge: »Kom.»'),

 # Acts questions resolved against EFS 1926 and 1927: R321-R326.
 ('Acts', 2, 2):('hade dragit fram', 'hade farit fram'),
 ('Acts', 4, 1):('Medan de ännu talade', 'Medan de nu talade'),
 ('Acts', 4, 21):('utväg genom att straffa', 'utväg att straffa'),
 ('Acts', 5, 1):('Men en ung man', 'Men en man'),
 ('Acts', 5, 13):('folket höll dem i ära', 'folket höll dem i stor ära'),
 ('Acts', 13, 16):('I män av Israels hus', 'I män av Israel'),

 # Romans 16:26 confirmed by the user's print photograph: R327 (V136).
 ('Rom', 16, 26):('den eviga Gudens', 'den evige Gudens'),

 # First Corinthians 7:23 confirmed by the user's photograph: R328 (V139).
 ('1Cor', 7, 23):('betalningen är given', 'betalning är given'),

 # Further letter readings confirmed by EFS 1926: R329-R332 (James 4:15 above).
 ('Col', 1, 23):('och som blivit predikat', 'och som har blivit predikat'),
 ('2Thess', 1, 10):('kommit till att tro', 'kommit till tro'),
 ('1John', 2, 20):('I haven all kunskap', 'I haven alla kunskap'),

 # Revelation questions resolved against EFS 1926 and 1927: R333-R353.
 ('Rev', 1, 4):('frid från honom', 'frid ifrån honom'),
 ('Rev', 2, 14):('en stötesten för Israels barn', 'en stötesten till fall för Israels barn'),
 ('Rev', 4, 3):('såsom en smaragd', 'såsom smaragd'),
 ('Rev', 4, 9):('och tillbedja honom som lever', 'och som lever'),
 ('Rev', 5, 4):('befanns vara värdig', 'befanns värdig'),
 ('Rev', 6, 4):('friden bort från jorden', 'friden bort ifrån jorden'),
 ('Rev', 6, 11):('och åt dem blev tillsagt att de ännu en liten tid skulle giva sig till ro', 'och dem blev tillsagt att de ännu en liten tid skulle giva sig ro'),
 ('Rev', 6, 12):('svart som en sorgdräkt', 'svart såsom en sorgdräkt'),
 ('Rev', 6, 15):('och alla de rika', 'och de rika'),
 ('Rev', 7, 14):('du vet själv det', 'du själv vet det'),
 ('Rev', 8, 3):('rökelse var given åt honom', 'rökelse blev given åt honom'),
 ('Rev', 8, 10):('Då föll från himmelen', 'Då föll ifrån himmelen'),
 ('Rev', 9, 17):('tedde sig för min syn', 'tedde sig för mig i min syn'),
 ('Rev', 14, 1):('på Sions berg jämte det', 'på Sions berg och jämte det'),
 ('Rev', 15, 8):('hade fått sin fullbordan', 'hade nått sin fullbordan'),
 ('Rev', 17, 1):('kom och talade med mig', 'kom och talade till mig'),
 ('Rev', 20, 4):('och icke heller tagit dess märke', 'och icke hade tagit dess märke'),
 ('Rev', 21, 5):('dessa ord äro visa och sanna', 'dessa ord äro vissa och sanna'),
 ('Rev', 22, 1):('en ström med vatten', 'en ström med levande vatten'),
 ('Rev', 22, 15):('de måste alla stanna därutanför', 'de måste stanna därutanför'),

 # Gospel questions resolved against EFS 1926: R354-R368.
 ('Matt', 12, 26):(' Huru kan då hans rike hava bestånd?', ''),
 ('Matt', 12, 27):('Och om det är med Beelsebul som jag driver ut de onda andarna, med vem driva då edra egna anhängare ut dem? De skola alltså vara edra domare.', 'Huru kan då hans rike hava bestånd? Och om det är med Beelsebul som jag driver ut de onda andarna, med vem driva då edra egna anhängare ut dem? De skola alltså vara edra domare.'),
 ('Matt', 13, 39):('Skördemännen är änglar', 'Skördemännen äro änglar'),
 ('Matt', 18, 20):('två eller tre är församlade', 'två eller tre äro församlade'),
 ('Matt', 18, 25):('då denna icke kunde betala', 'då denne icke kunde betala'),
 ('Matt', 24, 42):('vår Herre', 'eder Herre'),
 ('Matt', 25, 15):('och for utrikes', 'och for så utrikes'),
 ('Mark', 1, 43):('Sedan vände Jesus', 'Sedan visade Jesus'),
 ('Mark', 2, 14):('Då steg han upp', 'Då stod han upp'),
 ('Mark', 4, 15):('Satan ock tager', 'Satan och tager'),
 ('Mark', 5, 8):('du orena ande', 'du orene ande'),
 ('Mark', 6, 9):('Sandaler finge de', 'Sandaler fingo de'),
 ('Mark', 8, 6):('Ock han tog', 'Och han tog'),
 ('Mark', 13, 17):('Och ve de som', 'Och ve dem som'),
 ('Mark', 14, 23):('tackade Gud ock gav', 'tackade Gud och gav'),
 ('Mark', 15, 8):('Folket kom ditupp', 'Folket kom nu ditupp'),

 # Luke 1-8 questions resolved against EFS 1926: R369-R382.
 ('Luke', 1, 2):('dem som själva', 'dem som från begynnelsen själva'),
 ('Luke', 1, 9):('hände det sig', 'hände sig'),
 ('Luke', 1, 41):('blev fylld av', 'blev uppfylld av'),
 ('Luke', 2, 20):('såsom det blivit', 'såsom det hade blivit'),
 ('Luke', 2, 24):('ett par duvor eller två unga turturduvor', 'ett par turturduvor eller två unga duvor'),
 ('Luke', 3, 11):('och en som har matförråd', 'och den som har matförråd'),
 ('Luke', 4, 18):('Han har satt mig', 'Han har sänt mig'),
 ('Luke', 4, 41):('tillsade dem att icke säga', 'tillstadde dem att icke säga'),
 ('Luke', 5, 3):('undervisade folket från båten', 'undervisade folket ifrån båten'),
 ('Luke', 6, 48):('eftersom det var så byggt', 'eftersom det var så väl byggt'),
 ('Luke', 8, 1):('vandrade han igenom landet', 'vandrade han genom landet'),
 ('Luke', 8, 36):('de som hade sett händelsen', 'de som hade åsett händelsen'),

 # Remaining Luke questions resolved against EFS 1926: R383-R390.
 ('Luke', 9, 22):('de skriftlärda', 'de skriftlärde'),
 ('Luke', 10, 2):('»Skörden', 'Och han sade till dem: »Skörden'),
 ('Luke', 19, 33):('frågade ägaren dem', 'frågade ägarna dem'),
 ('Luke', 23, 16):('medan jag har tuktat', 'sedan jag har tuktat'),
 ('Luke', 23, 31):('om han gör så', 'om man gör så'),

 # John questions resolved against EFS 1926: R391-R424.
 ('John', 1, 39):('vid den tionde timmen', 'vid tionde timmen'),
 ('John', 2, 10):('alltid sätta fram', 'alltid först sätta fram'),
 ('John', 2, 14):('Och när han fick', 'Och han fick'),
 ('John', 2, 21):('tempel han talade', 'tempel som han talade'),
 ('John', 3, 15):('så att', 'för att'),
 ('John', 3, 29):('hör honom', 'hör på honom'),
 ('John', 4, 6):('vid den sjätte timmen', 'vid sjätte timmen'),
 ('John', 4, 12):('Icke är du väl', 'Icke är väl du'),
 ('John', 4, 24):('de som tillbedja måste', 'de som tillbedja honom måste'),
 ('John', 4, 52):('vid den sjunde timmen', 'vid sjunde timmen'),
 ('John', 5, 21):('uppväcker döda', 'uppväcker de döda'),
 ('John', 6, 2):('följde efter honom', 'följde honom'),
 ('John', 6, 10):('lägga sig här', 'lägra sig här'),
 ('John', 6, 35):('aldrig törsta', 'aldrig någonsin törsta'),
 ('John', 7, 23):('hel och frisk', 'hel och hållen frisk'),
 ('John', 7, 46):('människa talat', 'människa så talat'),
 ('John', 8, 39):('Jesus svarade till dem', 'Jesus sade till dem'),
 ('John', 9, 22):('fruktade judarna', 'fruktade för judarna'),
 ('John', 10, 7):('Åter sade Jesus', 'Åter sade då Jesus'),
 ('John', 11, 25):('Jesus svarade till henne', 'Jesus sade till henne'),
 ('John', 11, 32):('hade du varit där', 'hade du varit här'),
 ('John', 11, 33):('såg jämväl att de', 'såg att jämväl de'),
 ('John', 11, 48):('Om vi skola låta', 'Om vi låta'),
 ('John', 11, 54):('öppet bland judarna', 'öppet omkring bland judarna'),
 ('John', 12, 35):('var han går', 'vart han går'),
 ('John', 12, 41):('talade med honom', 'talade om honom'),
 ('John', 13, 10):('behöver allenast', 'behöver sedan allenast'),
 ('John', 14, 21):('och jag skall uppenbara', 'och skall uppenbara'),
 ('John', 17, 3):('den enda sanne Guden', 'den ende sanne Guden'),
 ('John', 18, 13):('överstepräst det året', 'överstepräst för det året'),
 ('John', 18, 33):('Pilatus gick åter in', 'Pilatus gick då åter in'),
 ('John', 18, 37):('Så sade Pilatus', 'Då sade Pilatus'),
 ('John', 20, 1):('på första veckodagen, medan', 'på första veckodagen om morgonen, medan'),
 ('John', 20, 21):('så sänder ock jag eder', 'så sänder jag ock eder'),

}

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
