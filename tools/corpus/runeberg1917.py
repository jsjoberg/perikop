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
