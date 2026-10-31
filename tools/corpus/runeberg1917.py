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
 ('Hag',2,16):('till HERRENS tempel','till HERRENS tempel;')}

# Joshua: exact spans read against both EFS printings; details and unchecked human review
# in docs/reports/sv1917-errata-joshua.md.
REPLACE.update({
 ('Josh',1,3):('ort som eder fot beträder','ort, som eder fot beträder,'), # JOS001
 ('Josh',1,6):('land','land,'), # JOS002
 ('Josh',1,7):('lag som min tjänare Mose har givit dig','lag, som min tjänare Mose har givit dig,'), # JOS003
 ('Josh',1,11):('land','land,'), # JOS004
 ('Josh',2,1):('hus','hus,'), # JOS005
 ('Josh',2,3):('män','män,'), # JOS006
 ('Josh',2,4):('icke','icke,'), # JOS007
 ('Josh',2,5):('icke','icke,'), # JOS008
 ('Josh',2,7):('stadsporten','stadsporten,'), # JOS009
 ('Josh',2,9):('vet','vet,'), # JOS010
 ('Josh',2,10):('hört','hört,'), # JOS011
 ('Josh',2,11):('är Gud,','är Gud'), # JOS012
 ('Josh',2,12):('min','eder, så skolen I ock göra barmhärtighet med min'), # JOS013
 ('Josh',2,18):('fönster','fönster,'), # JOS014
 ('Josh',2,19):('huvud,','eget huvud,'), # JOS015
 ('Josh',2,24):('äro i','äro ock i'), # JOS016
 ('Josh',3,4):('veta','veta,'), # JOS017
 ('Josh',3,8):('förbundsarken','förbundsarken,'), # JOS018
 ('Josh',3,10):('förnimma','förnimma,'), # JOS019
 ('Josh',3,13):('prästerna som bära HERRENS, hela jordens Herres, ark stå stilla med sina fötter i','prästerna, som bära HERRENS, hela jordens Herres, ark, stå stilla med sina fötter i Jordans vatten, skall'), # JOS020
 ('Josh',3,14):('prästerna som buro förbundsarken','prästerna, som buro förbundsarken,'), # JOS021
 ('Josh',3,15):('de som buro arken','de, som buro arken,'), # JOS022
 ('Josh',3,16):('stannade det vatten','stannade det vatten,'), # JOS023
 ('Josh',3,17):('prästerna som buro HERRENS förbundsark','prästerna, som buro HERRENS förbundsark,'), # JOS024
 ('Josh',4,4):('män','män,'), # JOS025
 ('Josh',4,7):('betyda','betyda,'), # JOS026
 ('Josh',4,9):('prästerna som buro förbundsarken','prästerna, som buro förbundsarken,'), # JOS027
 ('Josh',4,10):('prästerna som buro arken','prästerna, som buro arken,'), # JOS028
 ('Josh',4,11):('ark över,','ark över'), # JOS029
 ('Josh',4,14):('fruktat','hade fruktat'), # JOS030
 ('Josh',4,16):('prästerna som bära vittnesbördets ark','prästerna, som bära vittnesbördets ark,'), # JOS031
 ('Josh',4,18):('prästerna som buro HERRENS förbundsark','prästerna, som buro HERRENS förbundsark,'), # JOS032
 ('Josh',4,19):('månaden','månaden,'), # JOS033
 ('Josh',4,20):('stenarna som de hade tagit ur Jordan','stenarna, som de hade tagit ur Jordan,'), # JOS034
 ('Josh',5,1):('hörde','hörde,'), # JOS035
 ('Josh',5,4):('orsaken varför Josua omskar dem','orsaken, varför Josua omskar dem,'), # JOS036
 ('Josh',5,6):('svor att han icke skulle låta dem se det land','svor, att han icke skulle låta dem se det land,'), # JOS037
 ('Josh',5,8):('kvar','kvar,'), # JOS038
 ('Josh',5,13):('hände sig','hände sig,'), # JOS039
 ('Josh',6,8):('jubelbasunerna framför HERREN','de sju jubelbasunerna framför HERREN,'), # JOS040
 ('Josh',6,10):('dag','dag,'), # JOS041
 ('Josh',6,13):('präster som buro de sju jubelbasunerna framför HERRENS ark','präster, som buro de sju jubelbasunerna framför HERRENS ark,'), # JOS042
 ('Josh',6,17):('leva, jämte alla som äro inne i hennes hus, därför att hon gömde de utskickade','leva jämte alla som äro inne i hennes hus, därför att hon gömde de utskickade,'), # JOS043
 ('Josh',6,22):('kvinnan,','kvinnan'), # JOS044
 ('Josh',6,23):('män som hade varit där såsom spejare ditin och förde ut Rahab, jämte hennes fader och moder och hennes bröder och alla','män, som hade varit där såsom spejare, ditin och förde ut Rahab, jämte hennes fader och moder och hennes bröder och alla,'), # JOS045
 ('Josh',6,25):('utskickade','utskickade,'), # JOS046
 ('Josh',6,26):('man','man,'), # JOS047
 ('Josh',7,6):('kläder,','kläder'), # JOS048
 ('Josh',7,7):('giva i','giva oss i'), # JOS049
 ('Josh',7,11):('förbund','förbund,'), # JOS050
 ('Josh',7,18):('Sera','Sera,'), # JOS051
 ('Josh',7,19):('mig vad du','mig, vad du har'), # JOS052
 ('Josh',7,21):('begärelse','jag begärelse'), # JOS053
 ('Josh',7,24):('övrigt','övrigt,'), # JOS054
 ('Josh',8,6):('flyr','fly'), # JOS055
 ('Josh',8,10):('så, med de äldste i Israel i spetsen för folket,','så med de äldste i Israel i spetsen för folket'), # JOS056
 ('Josh',8,11):('krigsfolk som var kvar hos honom','krigsfolk, som var kvar hos honom,'), # JOS057
 ('Josh',8,13):('gick','som ock den del därav, som låg i försåt, väster om staden, gick'), # JOS058
 ('Josh',8,14):('själv visste nämligen icke','visste nämligen icke,'), # JOS059
 ('Josh',8,20):('folk som flydde åt öknen','folk, som flydde åt öknen,'), # JOS060
 ('Josh',8,21):('sågo','sågo,'), # JOS061
 ('Josh',8,33):('prästerna','prästerna,'), # JOS062
 ('Josh',9,1):('konungar som bodde på andra sidan Jordan, i Bergsbygden, i Låglandet och i hela kustlandet','konungar, som bodde på andra sidan Jordan, i Bergsbygden, i Låglandet och i hela Kustlandet'), # JOS063
 ('Josh',9,3):('hörde','hörde,'), # JOS064
 ('Josh',9,13):('skor som vi hava på oss','skor, som vi hava på oss,'), # JOS065
 ('Josh',9,16):('höra','höra,'), # JOS066
 ('Josh',9,19):('menigheten:','hela menigheten:'), # JOS067
 ('Josh',9,20):('är vad vi vilja göra med dem, i det att vi låta dem leva, på det att icke förtörnelse må komma över oss, för edens skull','är, vad vi vilja göra med dem, i det att vi låta dem leva, på det att icke förtörnelse må komma över oss för edens skull,'), # JOS068
 ('Josh',9,21):('sade till dem','sade till dem,'), # JOS069
 ('Josh',9,24):('tjänare huru HERREN, din Gud, hade tillsagt sin tjänare Mose','tjänare, huru HERREN, din Gud, hade tillsagt sin tjänare Mose,'), # JOS070
 ('Josh',9,27):('dagen att de skulle bliva vedhuggare och vattenbärare','dagen, att de skulle bliva vedhuggare och vattenbärare åt menigheten'), # JOS071
 ('Josh',10,1):('hörde','hörde,'), # JOS072
 ('Josh',10,6):('församlat','alla församlat'), # JOS073
 ('Josh',10,10):('nedgjorde dem,','nedgjorde dem'), # JOS074
 ('Josh',10,11):('så, under sin flykt för Israel,','så under sin flykt för Israel'), # JOS075
 ('Josh',10,12):('dag','dag,'), # JOS076
 ('Josh',10,17):('till','vid'), # JOS077
 ('Josh',10,19):('fiender, och nedgören dem som bliva efter; låten dem icke komma in i sina städer, ty HERREN, eder Gud','fiender och nedgören dem som bliva efter; låten dem icke komma in i sina städer, ty HERREN, eder Gud,'), # JOS078
 ('Josh',10,23):('Lakis,','Lakis'), # JOS079
 ('Josh',10,24):('sätt','sätten'), # JOS080
 ('Josh',10,25):('sina fiender','fiender'), # JOS081
 ('Josh',10,26):('fingo på','fingo hänga på'), # JOS082
 ('Josh',10,27):('i grottan','i grottan,'), # JOS083
 ('Josh',10,40):('andra','anda'), # JOS084
 ('Josh',11,3):('så och','så ock'), # JOS085
 ('Josh',11,5):('vatten,','vatten'), # JOS086
 ('Josh',11,9):('dem','dem,'), # JOS087
 ('Josh',11,15):('Josua;','ock Josua;'), # JOS088
 ('Josh',11,19):('hivéer som bodde i Gibeon, fanns ingen stad','hivéer, som bodde i Gibeon, fanns ingen stad,'), # JOS089
 ('Josh',11,20):('det att de förstockade sina hjärtan och mötte Israel med krig, för att de skulle givas till spillo,','det, att de förstockade sina hjärtan och mötte Israel med krig, för att de skulle givas till spillo'), # JOS090
 ('Josh',12,2):('Aroer vid bäcken Arnons strand och från dalens mitt,','från Aroer vid bäcken Arnons strand och från dalens mitt'), # JOS091
 ('Josh',13,1):('del','del,'), # JOS092
 ('Josh',13,3):('Kananéernas','Kanaans'), # JOS093
 ('Josh',13,4):('område.','område,'), # JOS094
 ('Josh',13,32):('arvslotter','arvslotter,'), # JOS095
 ('Josh',14,1):('arvslotter','arvslotter,'), # JOS096
 ('Josh',14,3):('leviterna','åt leviterna'), # JOS097
 ('Josh',14,5):('landet','landet.'), # JOS098
 ('Josh',14,9):('land som din fot har beträtt','land, som din fot har beträtt,'), # JOS099
 ('Josh',14,11):('stark som jag var den dag','stark, som jag var den dag,'), # JOS100
 ('Josh',14,12):('bergsbygd om vilken HERREN talade på den dagen. Du hörde ju själv då','bergsbygd, om vilken HERREN talade på den dagen. Du hörde ju själv då,'), # JOS101
 ('Josh',15,21):('städer','städer,'), # JOS102
 ('Josh',15,60):('byar;','byar.'), # JOS103
 ('Josh',16,9):('de städer','de städer,'), # JOS104
 ('Josh',16,10):('icke kananéerna','icke kananéerna,'), # JOS105
 ('Josh',17,1):('Och Manasse','Och Manasses'), # JOS106
 ('Josh',17,2):('Hefers barn,','Hefers barn'), # JOS107
 ('Josh',17,5):('lotter','lotter,'), # JOS108
 ('Josh',17,16):('kananéer som bo i dalbygden','kananéer, som bo i dalbygden,'), # JOS109
 ('Josh',18,2):('stammar','stammar,'), # JOS110
 ('Josh',18,3):('land','land,'), # JOS111
 ('Josh',18,8):('åstad bjöd Josua dem','åstad, bjöd Josua dem,'), # JOS112
 ('Josh',18,9):('upp','i en skrivelse upp'), # JOS113
 ('Josh',18,11):('område som lotten gav dem','område, som lotten gav dem,'), # JOS114
 ('Josh',18,21):('städer','städer,'), # JOS115
 ('Josh',19,8):('byar','byar,'), # JOS116
 ('Josh',19,11):('dalen','dalen,'), # JOS117
 ('Josh',19,39):('arvedel','arvedel,'), # JOS118
 ('Josh',19,50):('den stad','den stad,'), # JOS119
 ('Josh',19,51):('arvslotter','arvslotter,'), # JOS120
 ('Josh',20,2):('fristäder','fristäder,'), # JOS121
 ('Josh',20,3):('dråpare som ouppsåtligen, utan vett och vilja, har dödat någon','dråpare, som ouppsåtligen, utan vett och vilja, har dödat någon,'), # JOS122
 ('Josh',20,4):('plats','plats,'), # JOS123
 ('Josh',20,5):('nästa,','nästa'), # JOS124
 ('Josh',20,6):('från','och sitt hus igen, till den stad från'), # JOS125
 ('Josh',20,7):('därtill','härtill'), # JOS126
 ('Josh',20,8):('Gilead,','Gilead'), # JOS127
 ('Josh',20,9):('städer','städer,'), # JOS128
 ('Josh',21,2):('land, och sade: »HERREN bjöd genom Mose','land och sade: »HERREN bjöd genom Mose,'), # JOS129
 ('Josh',21,3):('HERRES','HERRENS'), # JOS130
 ('Josh',21,12):('byarna som hörde till staden','byarna, som hörde till staden,'), # JOS131
 ('Josh',21,19):('städer som Arons söner, prästerna, fingo','städer, som Arons söner, prästerna, fingo,'), # JOS132
 ('Josh',21,26):('städer som de övriga Kehats barns släkter fingo','städer, som de övriga Kehats barns släkter fingo,'), # JOS133
 ('Josh',21,33):('tretton städer,','tretton städer'), # JOS134
 ('Josh',21,35):('städer.','städer;'), # JOS135
 ('Josh',21,40):('De städer','De städer,'), # JOS136
 ('Josh',21,43):('land','land,'), # JOS137
 ('Josh',22,3):('hållit','hållit,'), # JOS138
 ('Josh',22,9):('tillbaka,','tillbaka'), # JOS139
 ('Josh',22,16):('otrohet som I haven begått mot Israels Gud, då I','otrohet, som I haven begått mot Israels Gud, då I nu'), # JOS140
 ('Josh',22,17):('nog','nog,'), # JOS141
 ('Josh',22,20):('ende som förgicks genom den missgärningen?','ende, som förgicks genom den missgärningen?»'), # JOS142
 ('Josh',22,24):('tänkte','tänkte,'), # JOS143
 ('Josh',22,27):('efterkommande','våra efterkommande'), # JOS144
 ('Josh',22,28):('händer','händer,'), # JOS145
 ('Josh',22,30):('hörde','hörde,'), # JOS146
 ('Josh',22,31):('förnummit','förnummit,'), # JOS147
 ('Josh',22,33):('dem, för att fördärva det land','dem för att fördärva det land,'), # JOS148
 ('Josh',23,2):('tillsyningsmän','dess tillsyningsmän'), # JOS149
 ('Josh',23,3):('ty HERREN','ty HERREN,'), # JOS150
 ('Josh',23,4):('folk som ännu äro kvar','folk, som ännu äro kvar,'), # JOS151
 ('Josh',23,6):('allt','allt,'), # JOS152
 ('Josh',23,7):('folk','folk,'), # JOS153
 ('Josh',23,12):('honom,','honom'), # JOS154
 ('Josh',23,13):('veta','veta,'), # JOS155
 ('Josh',23,14):('själ','själ,'), # JOS156
 ('Josh',23,15):('goda som HERREN, eder Gud, lovade eder','goda, som HERREN, eder Gud, lovade eder,'), # JOS157
 ('Josh',24,7):('ögon','ögon,'), # JOS158
 ('Josh',24,13):('land varpå du icke hade nedlagt något arbete, så ock städer','land, varpå du icke hade nedlagt något arbete, så ock städer,'), # JOS159
 ('Josh',24,14):('gudar','gudar,'), # JOS160
 ('Josh',24,15):('dag vem I viljen tjäna, antingen de gudar som edra fäder tjänade, när de bodde på andra sidan floden, eller de gudar','dag, vem I viljen tjäna, antingen de gudar, som edra fäder tjänade, när de bodde på andra sidan floden, eller de gudar,'), # JOS161
 ('Josh',24,17):('under, och bevarat oss på hela den väg vi hava vandrat och bland alla de folk','under och bevarat oss på hela den väg vi hava vandrat och bland alla de folk,'), # JOS162
 ('Josh',24,18):('amoréerna som bodde i landet. Därför vilja vi ock tjäna HERREN; ty an','amoréerna, som bodde i landet. Därför vilja vi ock tjäna HERREN; ty han'), # JOS163
 ('Josh',24,23):('gudar','gudar,'), # JOS164
})

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
