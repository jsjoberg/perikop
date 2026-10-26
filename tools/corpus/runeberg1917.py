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
 ('Luke', 1, 73):('lovat för vår fader Abraham,', 'lovat för vår fader Abraham.'),
 ('Luke', 3, 4):('i öknen: Bereden vägen', "i öknen: 'Bereden vägen"),
 ('Luke', 3, 9):('och kastat på elden.', 'och kastat på elden.»'),
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
