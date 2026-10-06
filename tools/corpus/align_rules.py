"""Reviewed versification knowledge for align_editions.py.

UNITS lists the verse sequences that align as one unit, in target order.
RULES state numbering differences: 'Book c:v1-v2=c:v' maps a verse range with a
constant offset, 'Book c1-c2=c' shifts whole chapters, '=Book c:v' changes book,
and '=-' marks verses without an expected counterpart. The first matching rule
applies; unlisted verses keep their numbers. OVERRIDES state links that a monotonic
alignment cannot find, such as transposed verses: 'refs > refs' links all source refs
with all target refs, 'range >> range' links verse by verse, and '-' means none.
"""
SV,GR,KJV,WEB='sv1917','grc-lxx','en-kjv','en-web'
OT=['Gen','Exod','Lev','Num','Deut','Josh','Judg','Ruth','1Sam','2Sam','1Kgs','2Kgs','1Chr','2Chr','Ezra','Job','Ps','Prov','Eccl','Song','Isa','Jer','Lam','Ezek','Dan','Hos','Joel','Amos','Obad','Jonah','Micah','Nah','Hab','Zeph','Hag','Zech','Mal','EsthGr']
APOCRYPHA=['Tob','Jdt','Wis','Sir','Baruch','EpJer','Sus','Bel','1Macc','2Macc','PrMan']

# Hebrew chapter divisions, which the Septuagint edition follows and the 1917 Bible does not.
HEBREW={
 'Gen':['Gen 31:55=32:1','Gen 32:1-32=32:2'],
 'Exod':['Exod 8:1-4=7:26','Exod 8:5-32=8:1','Exod 22:1=21:37','Exod 22:2-31=22:1'],
 'Lev':['Lev 6:1-7=5:20','Lev 6:8-30=6:1'],
 'Num':['Num 13:1=12:16','Num 13:2-34=13:1','Num 16:36-50=17:1','Num 17:1-13=17:16'],
 'Deut':['Deut 12:32=13:1','Deut 13:1-18=13:2','Deut 22:30=23:1','Deut 23:1-25=23:2'],
 '1Sam':['1Sam 20:43=21:1','1Sam 21:1-15=21:2'],
 '2Sam':['2Sam 18:33=19:1','2Sam 19:1-43=19:2'],
 '1Kgs':['1Kgs 4:21-34=5:1','1Kgs 5:1-18=5:15'],
 '2Kgs':['2Kgs 11:21=12:1','2Kgs 12:1-21=12:2'],
 '1Chr':['1Chr 6:1-15=5:27','1Chr 6:16-81=6:1','1Chr 12:5-40=12:6'],
 '2Chr':['2Chr 2:1=1:18','2Chr 2:2-18=2:1','2Chr 14:1=13:23','2Chr 14:2-15=14:1'],
 'Ezra':['Neh 4:1-6=Ezra 13:33','Neh 4:7-23=Ezra 14:1','Neh 9:38=Ezra 20:1','Neh 10:1-39=Ezra 20:2','Neh 1-13=Ezra 11'],
 'Job':['Job 39:1-3=38:39','Job 39:4-33=39:1','Job 39:34-38=40:1','Job 40:1-27=40:6','Job 40:28=41:1','Job 41:1-25=41:2'],
 'Eccl':['Eccl 7:1=6:12','Eccl 7:2-30=7:1'],
 'Song':['Song 5:17=6:1','Song 6:1-11=6:2','Song 6:12=7:1','Song 7:1-13=7:2'],
 'Isa':['Isa 9:1=8:23','Isa 9:2-21=9:1','Isa 64:1=63:19','Isa 64:2-12=64:1'],
 'Ezek':['Ezek 20:45-49=21:1','Ezek 21:1-32=21:6'],
 'Hos':['Hos 1:10-11=2:1','Hos 2:1-23=2:3','Hos 11:12=12:1','Hos 12:1-14=12:2'],
 'Joel':['Joel 2:28-32=3:1','Joel 3-3=4'],
 'Micah':['Micah 5:1=4:14','Micah 5:2-15=5:1'],
 'Nah':['Nah 1:15=2:1','Nah 2:1-13=2:2'],
 'Hag':['Hag 2:1=1:15','Hag 2:2-24=2:1'],
 'Zech':['Zech 1:18-21=2:1','Zech 2:1-13=2:5'],
 'Mal':['Mal 4:1-6=3:19'],
}
# The Septuagint numbers the Psalms differently from the Hebrew text.
PSALMS=['Ps 10:1-18=9:22','Ps 11-113=10','Ps 114:1-8=113:1','Ps 115:1-18=113:9','Ps 116:1-9=114:1','Ps 116:10-19=115:1','Ps 117-146=116','Ps 147:1-11=146:1','Ps 147:12-20=147:1']
# The Septuagint orders Jeremiah's oracles against the nations after 25:13.
JER_ORDER=[('Jer','1-24'),('Jer','25:1-13'),('Jer','49:34-39'),('Jer','46'),('Jer','50'),('Jer','51'),('Jer','47'),('Jer','49:7-22'),('Jer','49:1-6'),('Jer','49:28-33'),('Jer','49:23-27'),('Jer','48'),('Jer','25:14-38'),('Jer','26-45'),('Jer','52')]
JER_RULES=['Jer 9:1=8:23','Jer 9:2-26=9:1','Jer 25:14-38=32:14','Jer 46-46=26','Jer 50-51=27','Jer 47-47=29','Jer 48-48=31','Jer 26-44=33','Jer 45:1-5=51:31',
 'Jer 49:34-39=25:14','Jer 49:7-22=30:1','Jer 49:1-5=30:17','Jer 49:6=-','Jer 49:28-33=30:23','Jer 49:23-27=30:29']
JER_OVERRIDES=['Jer 23:7 > Jer 23:40a','Jer 23:8 > Jer 23:40b','Jer 31:35 > Jer 38:36','Jer 31:36 > Jer 38:37','Jer 31:37 > Jer 38:35',
 'Jer 49:34 > Jer 25:14 Jer 25:20','Jer 46:1 > -','Jer 10:5 > Jer 10:5 Jer 10:9a','Jer 10:10 > -','Jer 30:15 > -','Jer 30:16 > Jer 37:16']
# The Septuagint places Proverbs 30:1-14, 24:23-34, 30:15-33 and 31:1-9 after 24:22.
PROV_ORDER=[('Prov','1-24:22'),('Prov','30:1-14'),('Prov','24:23-34'),('Prov','30:15-33'),('Prov','31:1-9'),('Prov','25-29'),('Prov','31:10-31')]
PROV_RULES=['Prov 30:1-14=-','Prov 30:15-33=24:35','Prov 31:1-9=24:54']
PROV_OVERRIDES=['Prov 30:%d > Prov 24:22%s'%(v,letter) for v,letter in zip(range(1,15),'fghiklmnopqrst')]+['Prov 16:17 > Prov 16:17','Prov 18:24 > -',
 'Prov 20:10-13 >> Prov 20:10-13','Prov 20:14-19 > -','Prov 20:20 > Prov 20:9a','Prov 20:21 > Prov 20:9b','Prov 20:22 > Prov 20:9c']
# Rahlfs and Brenton number Exodus 36-39 in the Septuagint's own order and wording.
EXOD_OVERRIDES=['Exod 28:23 > -','Exod 28:24 > Exod 28:29a','Exod 28:25-28 > -','Exod 28:29 > Exod 28:29','Exod 23:22 > Exod 23:22',
 'Exod 36:8-9 >> Exod 37:1-2','Exod 36:10-34 > -','Exod 36:35-38 >> Exod 37:3-6','Exod 37:1-3 >> Exod 38:1-3','Exod 37:4 Exod 37:5 > Exod 38:4',
 'Exod 37:6-9 >> Exod 38:5-8','Exod 37:10 Exod 37:11 Exod 37:12 > Exod 38:9','Exod 37:13 Exod 37:14 > Exod 38:10','Exod 37:15-18 >> Exod 38:11-14',
 'Exod 37:19 Exod 37:20 Exod 37:21 > Exod 38:15','Exod 37:22 > Exod 38:16','Exod 37:23 Exod 37:24 > Exod 38:17','Exod 37:25-28 > -','Exod 37:29 > Exod 38:25',
 '- > Exod 38:18-21','Exod 38:1 Exod 38:2 > Exod 38:22','Exod 38:3 > Exod 38:23','Exod 38:4 Exod 38:5 Exod 38:6 Exod 38:7 > Exod 38:24','Exod 38:8 > Exod 38:26','- > Exod 38:27',
 'Exod 38:9-20 >> Exod 37:7-18','Exod 38:21-23 >> Exod 37:19-21','Exod 38:24-26 >> Exod 39:1-3','Exod 38:27 > Exod 39:4 Exod 39:5','Exod 38:28 > Exod 39:6',
 'Exod 38:29 > Exod 39:7','Exod 38:30 > Exod 39:8 Exod 39:10','Exod 38:31 > Exod 39:9','Exod 39:1 > Exod 36:8 Exod 39:13','Exod 39:2-31 >> Exod 36:9-38',
 'Exod 39:32 > Exod 39:11','- > Exod 39:12','Exod 39:33 > Exod 39:14','Exod 39:34 Exod 39:35 > -','Exod 39:36 > Exod 39:18','Exod 39:37 > Exod 39:17',
 'Exod 39:38 > Exod 39:16','Exod 39:39 > Exod 39:15','Exod 39:40 > Exod 39:20 Exod 39:21','Exod 39:41 > Exod 39:19','Exod 39:42 > Exod 39:22','Exod 39:43 > Exod 39:23']
# 3 Kingdoms places the palace (MT 7:1-12) after the temple furnishings and swaps chapters 20 and 21.
KGS_ORDER=[('1Kgs','1-6'),('1Kgs','7:13-51'),('1Kgs','7:1-12'),('1Kgs','8-19'),('1Kgs','21'),('1Kgs','20'),('1Kgs','22')]
KGS_RULES=['1Kgs 7:13-51=7:1','1Kgs 7:1-12=7:38','1Kgs 21-21=20','1Kgs 20-20=21']
KGS_OVERRIDES=['1Kgs 3:1 > 1Kgs 2:35c','1Kgs 4:20 > 1Kgs 2:46a','1Kgs 4:21 > 1Kgs 5:1','1Kgs 5:17 > 1Kgs 6:1a','1Kgs 5:18 > 1Kgs 6:1b 1Kgs 5:32',
 '1Kgs 6:1 > 1Kgs 6:1','1Kgs 6:37 > 1Kgs 6:1c','1Kgs 6:38 > 1Kgs 6:1d','1Kgs 8:12 1Kgs 8:13 > 1Kgs 8:53a','1Kgs 11:14 > 1Kgs 11:14',
 '1Kgs 11:43 1Kgs 12:2 > 1Kgs 11:43','1Kgs 12:1 > 1Kgs 12:1','1Kgs 12:3 > 1Kgs 12:3','1Kgs 12:24 > 1Kgs 12:24']
# Greek Esther places its additions inside the book; Tillägg till Ester prints them separately.
ESTHER_ORDER=[('EsthGr','1'),('Esth','1:1-3:13'),('EsthGr','2'),('Esth','3:14-4:17'),('EsthGr','3-4'),('EsthGr','5'),('Esth','5-8:12'),('EsthGr','6'),('Esth','8:13-10:3'),('EsthGr','7')]
ESTHER_OVERRIDES=['EsthGr 5:1 Esth 5:1 > EsthGr 5:1','EsthGr 5:12 Esth 5:2 > EsthGr 5:2','EsthGr 5:9-11 > EsthGr 5:1f','EsthGr 5:13-14 > EsthGr 5:2a']
# Theodotion's Daniel includes the Prayer and Song of the Three Young Men at 3:24-90.
DANIEL_ORDER=[('Dan','1-3:23'),('PrAzar',None),('Dan','3:24-12:99')]
DANIEL_RULES=['Dan 3:24-30=Dan 3:91','PrAzar 1:1-65=-','Dan 3:31-33=Dan 4:1','Dan 4:1-34=Dan 4:4','Dan 5:31=Dan 6:1','Dan 6:1-28=Dan 6:2']
# Greek Sirach transposes 30:25-33:16a and 33:16b-36:13 of the Latin order, which the 1921 Bible keeps.
SIR_ORDER=[('Sir','1-30:24'),('Sir','33:16-33'),('Sir','34'),('Sir','35'),('Sir','36:1-13'),('Sir','30:25'),('Sir','31'),('Sir','32'),('Sir','33:1-15'),('Sir','36:14-28'),('Sir','37-51')]
SIR_RULES=['Sir 33:16-33=-','Sir 34-35=-','Sir 36:1-13=-','Sir 30:25=33:12','Sir 31-31=34','Sir 32-32=35','Sir 33:1-15=36:1','Sir 36:14-28=36:17']
SV_GR=dict(HEBREW)
SV_GR.update({'Ps':PSALMS,'Jer':JER_RULES,'Prov':PROV_RULES,'1Kgs':HEBREW['1Kgs']+KGS_RULES,'Dan':DANIEL_RULES,'EsthGr':['EsthGr 1-7=-'],'Sir':SIR_RULES,
 'EpJer':['Baruch 6:1=EpJer 1:1','Baruch 6:2-72=EpJer 1:3'],
 'Tob':['Tob 4:8-18=-','Tob 5:9-22=5:8','Tob 6:1=5:22','Tob 6:2-18=6:1','Tob 7:12-17=7:13','Tob 10:8-12=10:9','Tob 13:8-10=-'],
 'Jdt':['Jdt 15:14=16:1','Jdt 16:1-6=16:2']})
UNITS={(SV,GR):{'Jer':{'src':JER_ORDER},'Prov':{'src':PROV_ORDER},'1Kgs':{'src':KGS_ORDER},'Ezra':{'src':[('Ezra',None),('Neh',None)]},
 'EsthGr':{'src':ESTHER_ORDER},'Dan':{'src':DANIEL_ORDER},'Sir':{'src':SIR_ORDER},'Baruch':{'src':[('Baruch','1-5')]},'EpJer':{'src':[('Baruch','6')]}}}
RULES={(SV,GR):SV_GR}
OVERRIDES={(SV,GR):{'Gen':['Gen 11:13 > Gen 11:13','Gen 35:16 Gen 35:21 > Gen 35:16'],'Exod':EXOD_OVERRIDES,'Deut':['Deut 32:43 > Deut 32:43','Deut 32:44 > Deut 32:44'],
 'Josh':[*['Josh 8:%d > Josh 9:2%s'%(v,letter) for v,letter in zip(range(30,36),'abcdef')],'Josh 9:1-2 >> Josh 9:1-2','Josh 19:47 > Josh 19:47a Josh 19:48 Josh 19:48a','Josh 19:48 > Josh 19:47','Josh 21:42 > Josh 21:42',
  'Josh 24:29 > Josh 24:30','Josh 24:30 > Josh 24:31','Josh 24:31 > Josh 24:29'],
 '1Sam':['1Sam 1:25 > 1Sam 1:25','1Sam 2:10 > 1Sam 2:10','1Sam 14:41 > 1Sam 14:41','1Sam 14:42 > 1Sam 14:42','1Sam 17:41 > -','1Sam 17:50 > -'],
 '2Sam':['2Sam 8:7 > 2Sam 8:7','2Sam 8:8 > 2Sam 8:8','2Sam 11:22 > 2Sam 11:22'],'1Kgs':KGS_OVERRIDES,
 '2Kgs':['2Kgs 1:16-18 >> 2Kgs 1:16-18','2Kgs 21:25 > 2Kgs 21:25 2Kgs 21:26'],'1Chr':['1Chr 12:4 > 1Chr 12:4 1Chr 12:5','1Chr 26:18 > 1Chr 26:18'],'2Chr':['2Chr 36:5 > 2Chr 36:5'],
 'Job':['Job 23:14 > -','Job 23:15 > Job 23:15','Job 36:28 > Job 36:28'],'Jer':JER_OVERRIDES,'Prov':PROV_OVERRIDES,'EsthGr':ESTHER_OVERRIDES,
 'Sir':['Sir 33:16 > Sir 30:25 Sir 36:16','Sir 36:12 > Sir 33:10','Sir 36:13 > Sir 33:11','Sir 30:25 > Sir 33:12'],
 'Ps':['Ps 14:3 > Ps 13:3','Ps 49:19 > Ps 48:19 Ps 48:20','Ps 49:20 > Ps 48:21'],'Tob':['Tob 5:8 Tob 5:9 > Tob 5:8']}}
# Swedish 1917 counts Psalm titles as verses, like the Hebrew text; the KJV includes them in verse 1.
TITLE_VERSES={3:1,4:1,5:1,6:1,7:1,8:1,9:1,12:1,13:1,18:1,19:1,20:1,21:1,22:1,30:1,31:1,34:1,36:1,38:1,39:1,40:1,41:1,42:1,44:1,45:1,46:1,47:1,48:1,49:1,
 51:2,52:2,53:1,54:2,55:1,56:1,57:1,58:1,59:1,60:2,61:1,62:1,63:1,64:1,65:1,67:1,68:1,69:1,70:1,75:1,76:1,77:1,80:1,81:1,83:1,84:1,85:1,88:1,89:1,92:1,102:1,108:1,140:1,142:1}
SV_KJV={'Ps':[rule for c,n in TITLE_VERSES.items() for rule in ('Ps %d:2-%d=%d:1'%(c,n+1,c),'Ps %d:%d-200=%d:2'%(c,n+2,c))],
 'Num':['Num 13:1=12:16','Num 13:2-34=13:1','Num 30:1=29:40','Num 30:2-17=30:1'],
 '1Sam':['1Sam 24:1=23:29','1Sam 24:2-23=24:1'],
 '1Kgs':['1Kgs 22:44=22:43','1Kgs 22:45-54=22:44'],
 'Job':['Job 39:1-3=38:39','Job 39:4-33=39:1','Job 39:34-38=40:1','Job 40:1-19=40:6','Job 40:20-28=41:1','Job 41:1-25=41:10'],
 'Eccl':['Eccl 4:17=5:1','Eccl 5:1-19=5:2','Eccl 7:1=6:12','Eccl 7:2-30=7:1'],
 'Song':['Song 5:17=6:1','Song 6:1-12=6:2'],
 'Dan':['Dan 3:31-33=4:1','Dan 4:1-34=4:4'],
 'Hos':['Hos 14:1=13:16','Hos 14:2-10=14:1'],
 'Jonah':['Jonah 2:1=1:17','Jonah 2:2-11=2:1'],
 'Hag':['Hag 2:1=1:15','Hag 2:2-24=2:1'],
 'Rev':['Rev 12:18=13:1']}
SV_KJV_OVERRIDES={'Ps':['Ps 13:6 > Ps 13:5 Ps 13:6','Ps 49:19 > Ps 49:18 Ps 49:19','Ps 49:20 > Ps 49:20'],'2Kgs':['2Kgs 21:25 > 2Kgs 21:25 2Kgs 21:26'],
 'Neh':['Neh 7:69 > Neh 7:69','Neh 7:70 > Neh 7:70'],'1Sam':['1Sam 3:22 1Sam 4:1 > 1Sam 4:1'],'Rev':['Rev 12:18 Rev 13:1 > Rev 13:1']}
RULES[(SV,KJV)]=SV_KJV
OVERRIDES[(SV,KJV)]=SV_KJV_OVERRIDES
CANON=['Gen','Exod','Lev','Num','Deut','Josh','Judg','Ruth','1Sam','2Sam','1Kgs','2Kgs','1Chr','2Chr','Ezra','Neh','Esth','Job','Ps','Prov','Eccl','Song','Isa','Jer','Lam','Ezek','Dan','Hos','Joel','Amos','Obad','Jonah','Micah','Nah','Hab','Zeph','Hag','Zech','Mal',
 'Matt','Mark','Luke','John','Acts','Rom','1Cor','2Cor','Gal','Eph','Phil','Col','1Thess','2Thess','1Tim','2Tim','Titus','Philemon','Heb','James','1Peter','2Peter','1John','2John','3John','Jude','Rev']
OLD_TESTAMENT=set(CANON[:39])
# WEB embeds most additions to Esther in long verses, and numbers Greek Daniel with Susanna and Bel as chapters 13-14.
UNITS[(SV,WEB)]={'EsthGr':{'src':ESTHER_ORDER,'keep':{'EsthGr'}},'DanGr':{'src':DANIEL_ORDER+[('Sus',None),('Bel',None)],'tgt':[('DanGr',None)],'keep':{'PrAzar','Sus','Bel'}}}
RULES[(SV,WEB)]={'EsthGr':['EsthGr 1-7=-'],'DanGr':['Dan 3:24-30=DanGr 3:91','PrAzar 1:1-65=-','Dan 3:31-33=DanGr 4:1','Dan 4:1-34=DanGr 4:4','Sus 1:1-64=DanGr 13:1','Bel 1:1-42=DanGr 14:1']}
OVERRIDES[(SV,WEB)]={'EsthGr':['EsthGr 1:1-17 > EsthGr 1:1','EsthGr 2:1-7 > EsthGr 3:13','EsthGr 3:1-11 >> EsthGr 4:18-28','EsthGr 4:1-19 >> EsthGr 4:29-47',
 'EsthGr 5:1-11 > EsthGr 5:1','EsthGr 5:12-16 > EsthGr 5:2','EsthGr 6:1-24 > EsthGr 8:13','EsthGr 7:1-11 >> EsthGr 10:4-14']}
UNITS[(WEB,GR)]={'Ps151':{'tgt':[('Ps','151')]}}
RULES[(WEB,GR)]={'Ps151':['Ps151 1-1=Ps 151']}
PAIRS={(SV,GR):OT+APOCRYPHA,(SV,KJV):CANON,(SV,WEB):['Tob','Jdt','Wis','Sir','Baruch','1Macc','2Macc','PrMan','EsthGr','DanGr'],(WEB,GR):['1Esd','3Macc','4Macc','Ps151']}
