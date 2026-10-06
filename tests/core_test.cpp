#include "core/model.hpp"
#include "storage/database.hpp"
#include "speech/speech.hpp"
#include <sqlite3.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <tuple>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <limits>
namespace {
int checks=0;
void check(bool value,const char* message) { ++checks;if(!value)throw std::runtime_error(message); }
ortho::CivilDate date(const char* value){return ortho::parse_date(value).value();}
}
int main(int argc,char** argv) {
    try {
        using namespace ortho;
        if(argc!=4)throw std::runtime_error("Usage: ortho-tests CORPUS USER CHART");
        check(shift_date(date("2024-02-28"),1)==date("2024-02-29"),"leap day");
        check(shift_date(date("2026-12-31"),1)==date("2027-01-01"),"year navigation");
        check(shift_date(date("2026-03-01"),-1)==date("2026-02-28"),"backwards navigation");
        check(!parse_date("2026-02-29"),"invalid leap date");
        check(!parse_date("2026-257-01")&&!parse_date("2026-01-257"),"overflow date parts");
        check(!parse_date("2026-10-05abc"),"trailing date text");
        check(date_swedish(date("2026-10-05"))=="Måndag 5 oktober 2026","Swedish civil date");
        SelectedDay selected(date("2026-10-05"));
        // Simulate a clock change and wake: no update method consults this clock.
        auto simulated_now=date("2026-10-06");
        check(selected.date()!=simulated_now && selected.date()==date("2026-10-05"),"midnight changes selected date");
        selected.move(1);check(selected.date()==simulated_now,"explicit date move");
        selected.select(date("2026-10-07"));check(selected.date()==date("2026-10-07"),"explicit selection");
        CorpusDb corpus(argv[1]);
        const auto paragraphs=corpus.paragraph_starts("sv1917","John");
        check(std::binary_search(paragraphs.begin(),paragraphs.end(),VerseRef{20,20}),"WEB editorial resurrection paragraph");
        check(!std::binary_search(paragraphs.begin(),paragraphs.end(),VerseRef{20,21}),"verse must not imply paragraph break");
        check(!std::binary_search(paragraphs.begin(),paragraphs.end(),VerseRef{20,14}),"internal prose break must not migrate to next verse");
        const auto greek_paragraphs=corpus.paragraph_starts("grc-lxx","Gen");
        check(std::binary_search(greek_paragraphs.begin(),greek_paragraphs.end(),VerseRef{1,6}),"original USFM Greek paragraph boundary");
        AntiochianLectionary lectionary(corpus);
        for(const auto& feast:corpus.feast_rules())for(const auto& name:{feast.title,feast.feast})
            if(!name.empty()&&!swedish_title(name))throw std::runtime_error("Missing Swedish title: "+name);
        check(swedish_title("Tuesday of the 19th week after Pentecost")=="Tisdag i 19:e veckan efter pingst","Swedish weekday title");
        check(swedish_title("21st Sunday after Pentecost")=="21:a söndagen efter pingst"&&swedish_title("11th Sunday after Pentecost")=="11:e söndagen efter pingst","Swedish ordinals");
        check(swedish_title("Sunday before Nativity – Eve of Nativity")=="Söndagen före Kristi födelse · Julafton","Coinciding Swedish titles");
        const auto today=lectionary.readings_for(date("2026-10-05"),CalendarStyle::New);
        check(today.readings.size()==2,"Antiochian daily pair");
        check(today.readings[0].passage.book=="Phil"&&today.readings[0].passage.first==VerseRef{1,1}&&today.readings[0].passage.last==VerseRef{1,7},"official Oct 5 epistle");
        check(today.readings[1].passage.book=="Luke"&&today.readings[1].passage.first==VerseRef{6,24}&&today.readings[1].passage.last==VerseRef{6,30},"official Oct 5 gospel");
        auto old=lectionary.readings_for(date("2026-10-06"),CalendarStyle::Old);
        auto modern=lectionary.readings_for(date("2026-10-06"),CalendarStyle::New);
        check(old.day.civil_date==modern.day.civil_date,"calendar shifts civil date");
        check(old.day.fixed_cycle=="2026-09-23"&&modern.day.fixed_cycle=="2026-10-06","independent fixed cycle");
        check(old.day.fixed_cycle&&old.day.paschal_cycle,"computed calendar cycles");
        check(!lectionary.readings_for(date("2030-01-01"),CalendarStyle::New).readings.empty(),"future recurring readings");
        check(orthodox_pascha(2026)==date("2026-04-12")&&orthodox_pascha(2027)==date("2027-05-02"),"official Pascha dates");
        check(fixed_calendar_date(date("2100-03-14"),CalendarStyle::Old)==std::chrono::year{2100}/2/29,"Julian century leap label");
        // Independent Serbian Church witnesses: published facts, not runtime lookups.
        check(fixed_calendar_date(date("2026-01-07"),CalendarStyle::Old)==date("2025-12-25"),"Serbian Nativity fixed date");
        check(fixed_calendar_date(date("2026-01-19"),CalendarStyle::Old)==date("2026-01-06"),"Serbian Theophany fixed date");
        check(fixed_calendar_date(date("2026-09-27"),CalendarStyle::Old)==date("2026-09-14"),"Serbian Elevation fixed date");
        const auto serbian_sunday=lectionary.readings_for(date("2026-01-11"),CalendarStyle::Old);
        check(serbian_sunday.readings[1].passage.book=="Matt"&&serbian_sunday.readings[1].passage.first==VerseRef{2,13}&&serbian_sunday.readings[1].passage.last==VerseRef{2,23},"Serbian Sunday after Nativity Gospel");
        const auto serbian_pascha=lectionary.readings_for(date("2026-04-12"),CalendarStyle::Old);
        check(serbian_pascha.readings[1].passage.book=="John"&&serbian_pascha.readings[1].passage.first==VerseRef{1,1}&&serbian_pascha.readings[1].passage.last==VerseRef{1,17},"Serbian published Pascha Gospel");
        check(fixed_calendar_date(date("2101-01-08"),CalendarStyle::Old)==date("2100-12-25"),"Julian conversion must not assume thirteen days forever");
        check(fixed_calendar_date(date("2800-02-29"),CalendarStyle::New)==date("2800-03-01"),"New calendar must use Revised Julian leap rules");
        check(fixed_calendar_date(date("2900-02-28"),CalendarStyle::New)==std::chrono::year{2900}/2/29,"Revised Julian century leap label");
        check(fixed_calendar_date(date("2900-03-01"),CalendarStyle::New)==date("2900-03-01"),"Revised Julian leap difference returns to zero");
        for(int year:{1,2036,2100,2400,2800,5000,9999}) {
            const auto start=std::chrono::year{year}/1/1;
            const int length=std::chrono::year{year}.is_leap()?366:365;
            for(int i=0;i<length;++i)for(auto style:{CalendarStyle::New,CalendarStyle::Old}) {
                const auto civil=shift_date(start,i);const auto computed=lectionary.readings_for(civil,style);
                check(computed.day.civil_date==civil&&!computed.readings.empty(),"calculated calendar horizon contains a gap");
            }
        }
        // The Julian computus repeats over 532 years; civil dates do not.
        for(int year=2000;year<2532;++year) {
            const auto a=fixed_calendar_date(orthodox_pascha(year),CalendarStyle::Old);
            const auto b=fixed_calendar_date(orthodox_pascha(year+532),CalendarStyle::Old);
            check(a.month()==b.month()&&a.day()==b.day(),"Julian 532-year Paschal cycle");
            check(std::chrono::weekday{std::chrono::sys_days{orthodox_pascha(year)}}==std::chrono::Sunday,"computed Pascha is not Sunday");
        }
        // Independently transcribed citations from the Archdiocese's official chart.
        const auto calculated_path=std::filesystem::path(argv[2]).parent_path()/"test-calculated-calendar.db";
        std::filesystem::copy_file(argv[1],calculated_path,std::filesystem::copy_options::overwrite_existing);
        sqlite3* calculated_db=nullptr;check(sqlite3_open(calculated_path.string().c_str(),&calculated_db)==SQLITE_OK,"calculated-only calendar fixture");
        check(sqlite3_exec(calculated_db,"DELETE FROM ordo_rule",nullptr,nullptr,nullptr)==SQLITE_OK,"remove every annual assignment");sqlite3_close(calculated_db);
        auto calculated_corpus=std::make_unique<CorpusDb>(calculated_path);
        AntiochianLectionary calculated_calendar(*calculated_corpus);
        std::ifstream chart(argv[3]);check(bool(chart),"official chart fixture missing");
        std::string line;int sundays=0;
        while(std::getline(chart,line)) {
            std::istringstream row(line);std::string iso,expected;std::getline(row,iso,'\t');
            const auto result=lectionary.readings_for(parse_date(iso).value(),CalendarStyle::New);
            const auto calculated=calculated_calendar.readings_for(parse_date(iso).value(),CalendarStyle::New);
            check(calculated.readings.size()==result.readings.size(),"annual table masks a recurring calculation error");
            for(std::size_t i=0;i<result.readings.size();++i) {
                const auto expected_parts=result.readings[i].segments(),parts=calculated.readings[i].segments();
                check(parts.size()==expected_parts.size(),"calculated-only reading segment count");
                for(std::size_t j=0;j<parts.size();++j)check(parts[j].book==expected_parts[j].book&&parts[j].first==expected_parts[j].first&&parts[j].last==expected_parts[j].last,"calculated-only reading differs from the official chart");
            }
            for(const auto& reading:result.readings) {
                check(bool(std::getline(row,expected,'\t')),"extra computed Sunday reading");
                std::string actual=std::to_string(int(reading.kind))+"=";bool first=true;
                for(const auto& p:reading.segments()) {
                    if(!first)actual+='|';first=false;
                    actual+=p.book+"_"+std::to_string(p.first.chapter*1000+p.first.verse)+"_"+std::to_string(p.last.chapter*1000+p.last.verse);
                }
                if(actual!=expected)throw std::runtime_error("Official chart mismatch "+iso+": "+actual+" != "+expected);
                ++checks;
            }
            check(!std::getline(row,expected,'\t'),"missing Sunday reading");++sundays;
        }
        check(sundays==52,"incomplete Sunday audit");
        calculated_corpus.reset();std::filesystem::remove(calculated_path);
        const auto thomas=lectionary.readings_for(date("2026-10-06"),CalendarStyle::New);
        check(thomas.readings.size()==2&&thomas.readings[0].passage.book=="1Cor"&&thomas.readings[1].passage.book=="John","Antiochian Apostle Thomas propers");
        check(orthodox_pascha(2028)==date("2028-04-16")&&orthodox_pascha(2029)==date("2029-04-08")&&orthodox_pascha(2030)==date("2030-04-28"),"official future Pascha dates");
        const auto pentecost=lectionary.readings_for(date("2026-05-31"),CalendarStyle::New).readings[1];
        check(pentecost.contains({7,37})&&!pentecost.contains({8,1})&&pentecost.contains({8,12}),"omitted Pentecost verses must stay omitted");
        for(int year=2027;year<=2035;++year) {
            const auto pascha=lectionary.readings_for(orthodox_pascha(year),CalendarStyle::New);
            check(pascha.readings.size()==2&&pascha.readings[0].passage.book=="Acts"&&pascha.readings[0].passage.first==VerseRef{1,1}&&pascha.readings[1].passage.book=="John","future computed Pascha");
        }
        auto passage=normalize_passage({"Luke",{6,36},{6,27}});
        check(passage&&passage->first==VerseRef{6,27},"normalize reverse range");
        check(passage->contains({6,31})&&!passage->contains({6,37}),"passage membership");
        check(!normalize_passage({"",{1,1},{1,1}}),"invalid empty book");
        check(source_for_language("el","Ps")=="grc-lxx","OT source selection");
        check(source_for_language("el","Luke")=="grc-patriarchal","NT source selection");
        check(source_for_language("xx","Luke").empty(),"unknown source selection");
        check(corpus.read_only(),"corpus not read-only");
        check(corpus.sources().size()==5,"source catalog");
        for(const auto& [book,ch,first,last]:std::vector<std::tuple<std::string,int,int,int>>{{"Ps",23,1,6},{"Ps",24,1,10},{"Luke",6,1,49},{"Phil",2,1,30}}) {
            for(int v=first;v<=last;++v)for(const std::string language:{"sv","el","en"}) {
                auto text=corpus.parallel_verse("sv1917",source_for_language(language,book),book,{ch,v});
                check(text&&!text->text.empty(),"required aligned fixture verse missing");
            }
        }
        check(!corpus.verse("sv1917","Luke",{99,1}),"missing text is not an error");
        check(!corpus.verse("unknown","Luke",{6,1}),"unknown source is not an error");
        auto map=corpus.alignment("sv1917","grc-lxx",{"Ps",{23,1},{23,6}});
        check(map&&map->kind==AlignmentKind::Renumbered&&map->to.book=="Ps","LXX mapping");
        auto greek=corpus.parallel_verse("sv1917","grc-lxx","Ps",{23,1});
        check(greek&&greek->ref.chapter==22&&greek->text.find("Κύριος")!=std::string::npos,"Greek Psalm alignment");
        auto psalm=corpus.map_passage("sv1917","grc-lxx",{"Ps",{51,1},{51,21}});
        check(psalm.size()==1&&psalm[0].first==VerseRef{50,1}&&psalm[0].last==VerseRef{50,21},"Hebrew and Greek Psalm titles share verse numbers");
        auto title=corpus.parallel_verse("sv1917","en-kjv","Ps",{22,1});
        check(title&&title->ref==VerseRef{22,1}&&title->text.find("My God")!=std::string::npos,"KJV includes the title in verse 1");
        auto kjv_shift=corpus.parallel_verse("sv1917","en-kjv","Ps",{22,3});
        check(kjv_shift&&kjv_shift->ref==VerseRef{22,2},"source-specific title numbering");
        check(corpus.parallel_verse("sv1917","en-kjv","Ps",{22,2}).error()=="Ingår i föregående vers","merged verse is shown once");
        check(corpus.parallel_verse("sv1917","grc-lxx","Ps",{100,1})->ref==VerseRef{99,1},"complete Psalter alignment");
        // The Septuagint orders Jeremiah differently and lacks about one eighth of the Hebrew text.
        check(corpus.parallel_verse("sv1917","grc-lxx","Jer",{31,31})->ref==VerseRef{38,31},"new covenant in LXX Jeremiah 38");
        check(corpus.parallel_verse("sv1917","grc-lxx","Jer",{46,2})->ref==VerseRef{26,2},"oracle against Egypt in LXX Jeremiah 26");
        check(corpus.parallel_verse("sv1917","grc-lxx","Jer",{25,15})->ref==VerseRef{32,15},"cup of wrath in LXX Jeremiah 32");
        check(corpus.parallel_verse("sv1917","grc-lxx","Jer",{33,14}).error()=="Saknas i Septuaginta","verse without LXX counterpart");
        check(corpus.parallel_verse("grc-lxx","sv1917","Jer",{23,40,"a"})->ref==VerseRef{23,7},"transposed LXX verse");
        check(corpus.parallel_verse("en-kjv","grc-lxx","Jer",{31,31})->ref==VerseRef{38,31},"KJV reaches the LXX through the Swedish alignment");
        auto nehemiah=corpus.parallel_verse("sv1917","grc-lxx","Neh",{1,1});
        check(nehemiah&&nehemiah->ref==VerseRef{11,1},"Nehemiah in Greek 2 Esdras 11");
        check(corpus.parallel_verse("sv1917","grc-lxx","Baruch",{6,2})->ref==VerseRef{1,3},"Letter of Jeremiah after its Greek title");
        // Lectionary references use KJV numbering, or the Septuagint's where the KJV has no such verse.
        auto joel=corpus.map_passage("en-kjv","grc-lxx",{"Joel",{2,28},{2,32}});
        check(joel.size()==1&&joel[0].first==VerseRef{3,1}&&joel[0].last==VerseRef{3,5},"Joel 2:28-32 is LXX 3:1-5");
        const auto rules=corpus.reading_rules();
        const auto song=std::find_if(rules.begin(),rules.end(),[](const auto& r){return r.reading.label.find("Song of the Three")!=std::string::npos;});
        check(song!=rules.end()&&song->reading.reference=="grc-lxx","Song of the Three uses Septuagint numbering");
        const auto daniel=corpus.localize(song->reading).segments();
        check(daniel.size()==2&&daniel[0].book=="Dan"&&daniel[0].last==VerseRef{3,23}&&daniel[1].book=="PrAzar","Holy Saturday Daniel reading in Swedish");
        const auto baruch=std::find_if(rules.begin(),rules.end(),[](const auto& r){return r.reading.passage.book=="Baruch";});
        check(baruch!=rules.end()&&corpus.localize(baruch->reading).passage.first==VerseRef{3,36},"Baruch 3:35 is Swedish 3:36");
        auto refs=corpus.coordinates("sv1917","Luke");
        check(refs.front().chapter==1&&refs.back().chapter==24,"adjacent chapter context");
        check(corpus.books().size()>=80,"full book catalog");
        check(corpus.verse("sv1917","Gen",{1,1})&&corpus.verse("sv1917","Rev",{22,21}),"Swedish full corpus endpoints");
        check(corpus.verse("sv1917","Wis",{1,1})&&corpus.verse("sv1917","Tob",{14,15}),"Swedish apocrypha");
        check(corpus.verse("grc-lxx","4Macc",{18,24})&&corpus.verse("grc-patriarchal","Rev",{22,21}),"Greek full corpus endpoints");
        check(corpus.verse("en-kjv","Gen",{1,1})&&corpus.verse("en-kjv","Rev",{22,21}),"English full corpus endpoints");
        check(corpus.verse("grc-lxx","Dan",{1,1})&&corpus.verse("grc-lxx","Dan",{12,13}),"Greek Daniel retained from DAG source");
        check(corpus.verse("grc-lxx","Gen",{31,50,"a"})&&corpus.verse("grc-lxx","EsthGr",{4,17,"z"}),"lettered LXX coordinates retained");
        const auto esther=corpus.coordinates("grc-lxx","EsthGr");
        const auto letter=std::find(esther.begin(),esther.end(),VerseRef{4,17,"a"});
        check(letter!=esther.end()&&letter!=esther.begin()&&*(letter-1)==VerseRef{4,17},"lettered portions retain their position in continuous Scripture");
        check(corpus.verse("en-web","Gen",{1,1})&&corpus.verse("en-web","Rev",{22,21}),"WEB complete Bible endpoints");
        for(const std::string book:{"Wis","Tob","3Macc","4Macc","1Esd","2Esd","Ps151","DanGr"})
            check(corpus.verse("en-web",book,{1,1}).has_value(),"English deuterocanonical content");
        auto joined=corpus.verse("en-web","4Macc",{8,29});
        check(joined&&joined->ref==VerseRef{8,28}&&joined->last==VerseRef{8,29},"joined publisher verse retains its complete range");
        check(source_for_language("en","Wis")=="en-web","English deuterocanonical selection");
        check(corpus.verse("grc-patriarchal","Luke",{2,23})->text.find("strong=")==std::string::npos,"nested USFM attributes leaked into display");
        sqlite3* readonly=nullptr;
        check(sqlite3_open_v2(argv[1],&readonly,SQLITE_OPEN_READONLY,nullptr)==SQLITE_OK,"read-only test connection");
        const int write_result=sqlite3_exec(readonly,"DELETE FROM verse",nullptr,nullptr,nullptr);sqlite3_close(readonly);
        check(write_result==SQLITE_READONLY,"read-only database accepted writes");
        auto lexicon=corpus.pronunciations("sv");
        auto utterance=make_utterance("Melkisedek, inte XMelkisedek eller Melkisedeks.","sv",lexicon);
        check(utterance.display_text=="Melkisedek, inte XMelkisedek eller Melkisedeks.","display text was mutated");
        check(utterance.speech_text=="Melki-sedek, inte XMelkisedek eller Melkisedeks.","token boundaries");
        check(make_utterance("MELKISEDEK!","sv",lexicon).speech_text=="Melki-sedek!","case insensitive pronunciation");
        lexicon.push_back({"sv","helige Ande","heliga ande","",200});
        lexicon.push_back({"sv","Åke","Oke","",100});
        check(make_utterance("helige Ande; helige, Ande; ÅKE.","sv",lexicon).speech_text=="heliga ande; helige, Ande; Oke.","phrase and Swedish token matching");
        check(make_utterance("Melkisedek","en",lexicon).speech_text=="Melkisedek","language isolation");
        std::string long_speech;
        for(int i=0;i<24;++i)long_speech+="Herren är min herde.  Ἐν ἀρχῇ ἦν ὁ λόγος.\n";
        std::string recovered;
        const auto compact=[](std::string s){std::erase_if(s,[](unsigned char c){return c==' '||c=='\n'||c=='\t'||c=='\r'||c=='\v'||c=='\f';});return s;};
        for(const auto& chunk:speech_chunks(long_speech)){check(!chunk.empty()&&chunk.size()<=240,"bounded speech chunks");recovered+=chunk;}
        check(compact(recovered)==compact(long_speech),"chunking must retain every Swedish and polytonic Greek byte in order");
        check(speech_chunks(std::string(300,'x'))==std::vector<std::string>{std::string(300,'x')},"a long word must never be cut into invalid pieces");
        StubSpeechEngine engine;engine.speak(utterance);check(engine.accepted.size()==1,"speech engine accepts utterance");
        engine.pause();check(engine.state==StubSpeechEngine::State::Paused,"speech pause");
        engine.resume();check(engine.state==StubSpeechEngine::State::Accepted,"speech resume");
        engine.stop();check(engine.accepted.empty()&&engine.state==StubSpeechEngine::State::Idle,"speech stop");
        check(reading_introduction(today.readings[0]).find("kapitel 1, vers 1 till 7")!=std::string::npos,"spoken reference");
        const std::filesystem::path user_path(argv[2]);std::filesystem::remove(user_path);
        {UserDb user(user_path);auto s=user.load();check(s.theme==Theme::System,"default theme");s.theme=Theme::Dark;s.calendar=CalendarStyle::Old;s.primary="el";s.parallel="en";s.font_size=24;s.speech_rate=175;user.save(s);}
        {UserDb user(user_path);auto s=user.load();check(s.theme==Theme::Dark&&s.calendar==CalendarStyle::Old&&s.primary=="el"&&s.parallel=="en"&&s.font_size==24&&s.speech_rate==175,"persisted settings");}
        std::filesystem::remove(user_path);
        sqlite3* legacy=nullptr;check(sqlite3_open(user_path.string().c_str(),&legacy)==SQLITE_OK,"legacy preference fixture");
        check(sqlite3_exec(legacy,"CREATE TABLE settings(key TEXT PRIMARY KEY,value TEXT NOT NULL); INSERT INTO settings VALUES('theme','dark'),('font_size','25');",nullptr,nullptr,nullptr)==SQLITE_OK,"legacy preference values");sqlite3_close(legacy);
        {UserDb user(user_path);auto s=user.load();check(s.theme==Theme::Dark&&s.font_size==25,"migration lost existing preferences");
         sqlite3* inspect=nullptr;sqlite3_open(user_path.string().c_str(),&inspect);
         sqlite3_stmt* query=nullptr;sqlite3_prepare_v2(inspect,"SELECT (SELECT user_version FROM pragma_user_version),(SELECT application_id FROM pragma_application_id),(SELECT strict FROM pragma_table_list WHERE name='settings'),(SELECT journal_mode FROM pragma_journal_mode)",-1,&query,nullptr);
         check(sqlite3_step(query)==SQLITE_ROW&&sqlite3_column_int(query,0)==1&&sqlite3_column_int(query,1)==0x4f525455&&sqlite3_column_int(query,2)==1&&std::string(reinterpret_cast<const char*>(sqlite3_column_text(query,3)))=="wal","settings storage policy");sqlite3_finalize(query);
         check(sqlite3_exec(inspect,"CREATE TRIGGER reject_setting BEFORE INSERT ON settings WHEN NEW.key='calendar' BEGIN SELECT RAISE(ABORT,'test interruption'); END",nullptr,nullptr,nullptr)==SQLITE_OK,"atomicity fixture");sqlite3_close(inspect);
         s.theme=Theme::Light;s.font_size=28;bool rejected=false;try{user.save(s);}catch(const std::exception&){rejected=true;}
         check(rejected&&user.load().theme==Theme::Dark&&user.load().font_size==25,"failed save must roll back every preference");}
        std::filesystem::remove(user_path);
        sqlite3_open(user_path.string().c_str(),&legacy);sqlite3_exec(legacy,"PRAGMA user_version=99",nullptr,nullptr,nullptr);sqlite3_close(legacy);
        bool future_rejected=false;try{UserDb future(user_path);}catch(const std::exception&){future_rejected=true;}
        check(future_rejected,"unknown future settings schema must not be overwritten");std::filesystem::remove(user_path);
        const auto cache_path=user_path.parent_path()/std::filesystem::path(u8"test-speech-Å.db");std::filesystem::remove(cache_path);
        const std::vector<float> pcm={0.0f,0.125f,-0.75f,1.0f,-1.0f};
        {SpeechCache cache(cache_path);check(!cache.load("voice-a","sv","Herren"),"empty speech cache");
         cache.save("voice-a","sv","Herren",pcm);
         check(cache.load("voice-a","sv","Herren")==pcm,"speech PCM round trip");
         check(!cache.load("voice-b","sv","Herren")&&!cache.load("voice-a","el","Herren")&&!cache.load("voice-a","sv","Ordet"),"speech cache must isolate model, language, and text");
         bool invalid=false;try{cache.save("voice-a","sv","Herren",{std::numeric_limits<float>::quiet_NaN()});}catch(const std::exception&){invalid=true;}
         check(invalid&&cache.load("voice-a","sv","Herren")==pcm,"invalid audio must not replace a valid cache entry");}
        {SpeechCache cache(cache_path);check(cache.load("voice-a","sv","Herren")==pcm,"speech cache persists across process restarts");}
        const auto cache_utf8=cache_path.u8string();sqlite3_open(reinterpret_cast<const char*>(cache_utf8.c_str()),&legacy);sqlite3_exec(legacy,"PRAGMA user_version=99",nullptr,nullptr,nullptr);sqlite3_close(legacy);
        future_rejected=false;try{SpeechCache cache(cache_path);}catch(const std::exception&){future_rejected=true;}
        check(future_rejected,"unknown future speech cache schema must not be overwritten");std::filesystem::remove(cache_path);
        std::cout<<checks<<" checks passed.\n";return 0;
    } catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}
}
