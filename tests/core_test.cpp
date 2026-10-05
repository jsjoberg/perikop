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
        AntiochianLectionary lectionary(corpus);
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
        // Independently transcribed citations from the Archdiocese's official chart.
        std::ifstream chart(argv[3]);check(bool(chart),"official chart fixture missing");
        std::string line;int sundays=0;
        while(std::getline(chart,line)) {
            std::istringstream row(line);std::string iso,expected;std::getline(row,iso,'\t');
            const auto result=lectionary.readings_for(parse_date(iso).value(),CalendarStyle::New);
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
        check(corpus.sources().size()==4,"source catalog");
        for(const auto& [book,ch,first,last]:std::vector<std::tuple<std::string,int,int,int>>{{"Ps",23,1,6},{"Ps",24,1,10},{"Luke",6,1,49},{"Phil",2,1,30}}) {
            for(int v=first;v<=last;++v)for(const std::string language:{"sv","el","en"}) {
                auto text=corpus.parallel_verse("sv1917",source_for_language(language,book),book,{ch,v});
                check(text&&!text->text.empty(),"required aligned fixture verse missing");
            }
        }
        check(!corpus.verse("sv1917","Luke",{99,1}),"missing text is not an error");
        check(!corpus.verse("unknown","Luke",{6,1}),"unknown source is not an error");
        auto map=corpus.alignment("sv1917","grc-lxx",{"Ps",{23,1},{23,6}});
        check(map&&map->kind==AlignmentKind::Renumbered&&map->to.first.chapter==22,"LXX mapping");
        auto greek=corpus.parallel_verse("sv1917","grc-lxx","Ps",{23,1});
        check(greek&&greek->ref.chapter==22&&greek->text.find("Κύριος")!=std::string::npos,"Greek Psalm alignment");
        auto split=corpus.parallel_verse("sv1917","grc-lxx","Ps",{22,31});
        check(split&&split->ref==VerseRef{21,31}&&split->last==VerseRef{21,32},"split verse coordinates");
        check(split->text.find("Καὶ ἀναγγελοῦσι")!=std::string::npos,"split verse text retrieval");
        auto kjv_shift=corpus.parallel_verse("sv1917","en-kjv","Ps",{22,2});
        check(kjv_shift&&kjv_shift->ref==VerseRef{22,1},"source-specific title numbering");
        check(!corpus.parallel_verse("sv1917","grc-lxx","Ps",{100,1}),"unverified LXX mapping");
        auto refs=corpus.coordinates("sv1917","Luke");
        check(refs.front().chapter==1&&refs.back().chapter==24,"adjacent chapter context");
        check(corpus.books().size()>=80,"full book catalog");
        check(corpus.verse("sv1917","Gen",{1,1})&&corpus.verse("sv1917","Rev",{22,21}),"Swedish full corpus endpoints");
        check(corpus.verse("sv1917","Wis",{1,1})&&corpus.verse("sv1917","Tob",{14,15}),"Swedish apocrypha");
        check(corpus.verse("grc-lxx","4Macc",{18,24})&&corpus.verse("grc-patriarchal","Rev",{22,21}),"Greek full corpus endpoints");
        check(corpus.verse("en-kjv","Gen",{1,1})&&corpus.verse("en-kjv","Rev",{22,21}),"English full corpus endpoints");
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
        StubSpeechEngine engine;engine.speak(utterance);check(engine.accepted.size()==1,"speech engine accepts utterance");
        engine.pause();check(engine.state==StubSpeechEngine::State::Paused,"speech pause");
        engine.resume();check(engine.state==StubSpeechEngine::State::Accepted,"speech resume");
        engine.stop();check(engine.accepted.empty()&&engine.state==StubSpeechEngine::State::Idle,"speech stop");
        check(reading_introduction(today.readings[0]).find("kapitel 1, vers 1 till 7")!=std::string::npos,"spoken reference");
        const std::filesystem::path user_path(argv[2]);std::filesystem::remove(user_path);
        {UserDb user(user_path);auto s=user.load();check(s.theme==Theme::System,"default theme");s.theme=Theme::Dark;s.calendar=CalendarStyle::Old;s.parallel="en";s.font_size=24;user.save(s);}
        {UserDb user(user_path);auto s=user.load();check(s.theme==Theme::Dark&&s.calendar==CalendarStyle::Old&&s.parallel=="en"&&s.font_size==24,"persisted settings");}
        std::filesystem::remove(user_path);
        std::cout<<checks<<" checks passed.\n";return 0;
    } catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}
}
