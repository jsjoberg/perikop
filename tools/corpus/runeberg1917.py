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
 # Ruth: RUT001–RUT026; both EFS print images, see sv1917-errata-ruth.md.
 ('Ruth',1,1):('På den tid då domarna regerade uppstod hungersnöd i landet. Då drog en man från Bet-Lehem i Juda åstad med sin hustru och sina båda söner för att bosätta sig i Moabs land under någon tid.','På den tid, då domarna regerade, uppstod hungersnöd i landet. Då drog en man från Bet-Lehem i Juda åstad med sin hustru och sina båda söner för att bosätta sig i Moabs land under någon tid.'),
 ('Ruth',1,6):('Då stod hon upp med sina sonhustrur för att vända tillbaka från Moabs land; ty hon hade hört i Moabs land att HERREN hade sett till sitt folk och givit det bröd.','Då stod hon upp med sina sonhustrur för att vända tillbaka från Moabs land; ty hon hade hört i Moabs land, att HERREN hade sett till sitt folk och givit det bröd.'),
 ('Ruth',1,8):('sade Noomi till sina båda sonhustrur: »Vänden om och gå hem igen, var och en till sin moder.','sade Noomi till sina båda sonhustrur: »Vänden om och gån hem igen, var och en till sin moder.'),
 ('Ruth',1,9):('HERREN bevise godhet mot eder, såsom I haven gjort mot de båda döda och mot mig. HERREN give eder att I mån finna ro, var i sin mans hus.» Därefter kysste hon dem. Men de brusto ut i gråt','HERREN bevise godhet mot eder, såsom I haven gjort mot de båda döda och mot mig. HERREN give eder, att I mån finna ro, var i sin mans hus.» Därefter kysste hon dem. Men de brusto ut i gråt'),
 ('Ruth',1,18):('Då hon nu såg att denna stod fast i sitt beslut att gå med henne, upphörde hon att tala därom med henne.','Då hon nu såg, att denna stod fast i sitt beslut att gå med henne, upphörde hon att tala därom med henne.'),
 ('Ruth',2,2):('Och moabitiskan Rut sade till Noomi: »Låt mig gå ut på åkern och plocka ax efter någon inför vilkens ögon jag finner nåd.» Hon svarade henne: »Ja, gå, min dotter.»','Och moabitiskan Rut sade till Noomi: »Låt mig gå ut på åkern och plocka ax efter någon, inför vilkens ögon jag finner nåd.» Hon svarade henne: »Ja, gå, min dotter.»'),
 ('Ruth',2,3):('Då gick hon åstad och kom till en åker och plockade där ax efter skördemännen; och det hände sig så för henne att åkerstycket tillhörde Boas, som var av Elimeleks släkt.','Då gick hon åstad och kom till en åker och plockade där ax efter skördemännen; och det hände sig så för henne, att åkerstycket tillhörde Boas, som var av Elimeleks släkt.'),
 ('Ruth',2,6):('Tjänaren som hade uppsikt över skördemännen svarade och sade: »Det är en moabitisk kvinna, den kvinna som med Noomi har kommit hit från Moabs land.','Tjänaren, som hade uppsikt över skördemännen, svarade och sade: »Det är en moabitisk kvinna, den kvinna, som med Noomi har kommit hit från Moabs land.'),
 ('Ruth',2,7):('Hon bad att hon skulle få plocka och hopsamla ax bland kärvarna, efter skördemännen; och så kom hon, och hon har hållit på allt sedan i morse ända till denna stund, utom att hon nyss har vilat något litet därinne.»','Hon bad, att hon skulle få plocka och hopsamla ax bland kärvarna efter skördemännen; och så kom hon, och hon har hållit på allt sedan i morse ända till denna stund, utom att hon nyss har vilat något litet därinne.»'),
 ('Ruth',2,11):('Boas svarade och sade till henne: »För mig har blivit berättat allt vad du har gjort mot din svärmoder efter din mans död, huru du har övergivit din fader och din moder och ditt fädernesland, och vandrat åstad till ett folk som du förut icke kände.','Boas svarade och sade till henne: »För mig har blivit berättat allt vad du har gjort mot din svärmoder efter din mans död, huru du har övergivit din fader och din moder och ditt fädernesland och vandrat åstad till ett folk som du förut icke kände.'),
 ('Ruth',2,12):('HERREN vedergälle dig för vad du har gjort; ja, må full lön tillfalla dig från HERREN, Israels Gud, till vilken du har kommit, för att finna tillflykt under hans vingar.»','HERREN vedergälle dig för vad du har gjort; ja, må full lön tillfalla dig från HERREN, Israels Gud, till vilken du har kommit för att finna tillflykt under hans vingar.»'),
 ('Ruth',2,18):('Och hon tog sin börda och gick in i staden, och hennes svärmoder fick se vad hon hade plockat. Därefter tog hon fram och gav henne vad hon hade fått över, sedan hon hade ätit sig mätt.','Och hon tog sin börda och gick in i staden, och hennes svärmoder fick se, vad hon hade plockat. Därefter tog hon fram och gav henne, vad hon hade fått över, sedan hon hade ätit sig mätt.'),
 ('Ruth',2,19):('Då sade hennes svärmoder till henne: »Var har du i dag plockat ax, och var har du arbetat? Välsignad vare han som har tagit sig an dig!» Då berättade hon för sin svärmoder hos vem hon hade arbetat; hon sade: »Den man som jag i dag har arbetat hos heter Boas.»','Då sade hennes svärmoder till henne: »Var har du i dag plockat ax, och var har du arbetat? Välsignad vare han som har tagit sig an dig!» Då berättade hon för sin svärmoder, hos vem hon hade arbetat; hon sade: »Den man, som jag i dag har arbetat hos, heter Boas.»'),
 ('Ruth',3,5):('Hon svarade: »Allt vad du säger vill jag göra.»','Hon svarade henne: »Allt vad du säger vill jag göra.»'),
 ('Ruth',3,11):('Så frukta nu icke, min dotter; allt vad du säger vill jag göra dig. Ty allt folket i min stad vet att du är en rättskaffens kvinna.','Så frukta nu icke, min dotter; allt vad du säger vill jag göra dig. Ty allt folket i min stad vet, att du är en rättskaffens kvinna.'),
 ('Ruth',3,13):('Stanna nu kvar i natt; om han i morgon vill taga dig efter bördesrätt, gott, må han då göra det, så sant HERREN lever. Ligg nu kvar ända till morgonen.»','Stanna nu kvar i natt; om han i morgon vill taga dig efter bördesrätt, gott, må han då göra det. Men har han icke lust att taga dig efter bördesrätt, då skall jag göra det, så sant HERREN lever. Ligg nu kvar ända till morgonen.»'),
 ('Ruth',3,14):('Så låg hon vid hans fötter ända till morgonen, men hon fick stå upp, innan ännu någon kunde känna igen den andre; ty han tänkte: »Det får icke bliva känt att kvinnan har kommit hit till tröskplatsen.»','Så låg hon vid hans fötter ända till morgonen, men hon fick stå upp, innan ännu någon kunde känna igen den andre; ty han tänkte: »Det får icke bliva känt, att kvinnan har kommit hit till tröskplatsen.»'),
 ('Ruth',3,15):('Och han sade: »Räck hit manteln som du har på dig, och håll fram den.» Och hon höll fram den. Då mätte han upp sex mått korn och gav henne att bära; därefter gick hon in i staden.','Och han sade: »Räck hit manteln, som du har på dig, och håll fram den.» Och hon höll fram den. Då mätte han upp sex mått korn och gav henne att bära; därefter gick hon in i staden.'),
 ('Ruth',3,18):('Då svarade hon: »Bida, min dotter, till dess du får se huru saken avlöper; ty mannen skall icke giva sig till ro, med mindre han i dag för saken till sitt slut.»','Då svarade hon: »Bida, min dotter, till dess du får se, huru saken avlöper; ty mannen skall icke giva sig till ro, med mindre han i dag för saken till sitt slut.»'),
 ('Ruth',4,1):('Och Boas hade gått upp till stadsporten och satt sig där. Då hände sig att den bördeman som Boas hade talat om gick där fram; då nämnde han honom vid namn och sade: »Kom hit och sätt dig här.» Och han kom och satte sig.','Och Boas hade gått upp till stadsporten och satt sig där. Då hände sig, att den bördeman, som Boas hade talat om, gick där fram; då nämnde han honom vid namn och sade: »Kom hit och sätt dig här.» Och han kom och satte sig.'),
 ('Ruth',4,3):('Sedan sade han till bördemannen: »Det åkerstycke som tillhörde vår broder Elimelek har Noomi sålt, hon som kom tillbaka från Moabs land.','Sedan sade han till bördemannen: »Det åkerstycke, som tillhörde vår broder Elimelek, har Noomi sålt, hon som kom tillbaka från Moabs land.'),
 ('Ruth',4,4):('Därefter tänkte jag att jag skulle underrätta dig därom och säga: Köp det inför dem som här sitta och inför mitt folks äldste. Om du vill taga det efter bördesrätt, så säg mig det, så att jag får veta det, ty ingen annan äger bördesrätt än du och, näst dig, jag själv.» Han sade: »Jag vill taga det efter bördesrätt.»','Därför tänkte jag, att jag skulle underrätta dig därom och säga: Köp det inför dem som här sitta och inför mitt folks äldste. Om du vill taga det efter bördesrätt, så gör det; men om du icke vill taga det efter bördesrätt, så säg mig det, så att jag får veta det, ty ingen annan äger bördesrätt än du och, näst dig, jag själv.» Han sade: »Jag vill taga det efter bördesrätt.»'),
 ('Ruth',4,7):('Men när någon bördade något eller avtalade ett byte, var det fordom sed i Israel att han, till stadfästelse av ett sådant avtal, drog av sig sin sko och gav den åt den andre; och detta gällde såsom ett vittnesbörd i Israel.','Men när någon bördade något eller avtalade ett byte, var det fordom sed i Israel, att han, till stadfästelse av ett sådant avtal, drog av sig sin sko och gav den åt den andre; och detta gällde såsom ett vittnesbörd i Israel.'),
 ('Ruth',4,9):('Då sade Boas till de äldste och till allt folket: »I ären i dag vittnen till att jag nu har köpt av Noomis hand allt vad som har tillhört Kiljon och Mahelon.','Då sade Boas till de äldste och till allt folket: »I ären i dag vittnen till att jag nu har köpt av Noomis hand allt vad som har tillhört Elimelek, och allt vad som har tillhört Kiljon och Mahelon.'),
 ('Ruth',4,11):('Och allt folket i stadsporten, så ock de äldste, svarade: »Ja, och HERREN låte den kvinna som nu går in i ditt hus bliva lik Rakel och Lea, de båda som hava byggt upp Israels hus. Och må du förkovra dig storligen i Efrata och göra dig ett namn i Bet-Lehem.','Och allt folket i stadsporten, så ock de äldste, svarade: »Ja, och HERREN låte den kvinna, som nu går in i ditt hus, bliva lik Rakel och Lea, de båda som hava byggt upp Israels hus. Och må du förkovra dig storligen i Efrata och göra dig ett namn i Bet-Lehem.'),
 ('Ruth',4,14):('Då sade kvinnorna till Noomi: »Lovad vare HERREN, som i dag har så gjort, att det icke fattas dig en bördeman som skall få ett namn i Israel!','Då sade kvinnorna till Noomi: »Lovad vare HERREN, som i dag har så gjort, att det icke fattas dig en bördeman, som skall få ett namn i Israel!'),
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
