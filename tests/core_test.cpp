#include "core/model.hpp"
#include "storage/database.hpp"
#include "speech/speech.hpp"
#include <sqlite3.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <tuple>
namespace {
int checks=0;
void check(bool value,const char* message) { ++checks;if(!value)throw std::runtime_error(message); }
ortho::CivilDate date(const char* value){return ortho::parse_date(value).value();}
}
int main(int argc,char** argv) {
    try {
        using namespace ortho;
        if(argc!=3)throw std::runtime_error("Usage: ortho-tests CORPUS USER");
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
        FixtureLectionary lectionary;
        check(lectionary.readings_for(date("2026-10-05"),CalendarStyle::New).readings.size()==3,"fixture readings");
        auto old=lectionary.readings_for(date("2026-10-06"),CalendarStyle::Old);
        auto modern=lectionary.readings_for(date("2026-10-06"),CalendarStyle::New);
        check(old.day.civil_date==modern.day.civil_date,"calendar shifts civil date");
        check(old.readings[0].passage.first.chapter==23&&modern.readings[0].passage.first.chapter==24,"distinct calendar fixtures");
        check(!old.day.fixed_cycle&&!old.day.paschal_cycle,"invented calendar cycle");
        check(lectionary.readings_for(date("2030-01-01"),CalendarStyle::New).readings.empty(),"fabricated unknown-date readings");
        auto passage=normalize_passage({"Luke",{6,36},{6,27}});
        check(passage&&passage->first==VerseRef{6,27},"normalize reverse range");
        check(passage->contains({6,31})&&!passage->contains({6,37}),"passage membership");
        check(!normalize_passage({"",{1,1},{1,1}}),"invalid empty book");
        check(source_for_language("el","Ps")=="grc-ot-fixture","OT source selection");
        check(source_for_language("el","Luke")=="grc-nt-fixture","NT source selection");
        check(source_for_language("xx","Luke").empty(),"unknown source selection");
        CorpusDb corpus(argv[1]);check(corpus.read_only(),"corpus not read-only");
        check(corpus.sources().size()==4,"source catalog");
        for(const auto& [book,ch,first,last]:std::vector<std::tuple<std::string,int,int,int>>{{"Ps",23,1,6},{"Ps",24,1,10},{"Luke",6,1,49},{"Phil",2,1,30}}) {
            for(int v=first;v<=last;++v)for(const std::string language:{"sv","el","en"}) {
                auto text=corpus.parallel_verse("sv1917",source_for_language(language,book),book,{ch,v});
                check(text&&!text->text.empty(),"required aligned fixture verse missing");
            }
        }
        check(!corpus.verse("sv1917","Luke",{99,1}),"missing text is not an error");
        check(!corpus.verse("unknown","Luke",{6,1}),"unknown source is not an error");
        auto map=corpus.alignment("sv1917","grc-ot-fixture",{"Ps",{23,1},{23,6}});
        check(map&&map->kind==AlignmentKind::Renumbered&&map->to.first.chapter==22,"LXX mapping");
        auto greek=corpus.parallel_verse("sv1917","grc-ot-fixture","Ps",{23,1});
        check(greek&&greek->ref.chapter==22&&greek->text.find("Κύριος")!=std::string::npos,"Greek Psalm alignment");
        auto split=corpus.parallel_verse("sv1917","grc-ot-fixture","Ps",{22,31});
        check(split&&split->ref==VerseRef{21,31}&&split->last==VerseRef{21,32},"split verse coordinates");
        check(split->text.find("Καὶ ἀναγγελοῦσι")!=std::string::npos,"split verse text retrieval");
        auto kjv_shift=corpus.parallel_verse("sv1917","en-kjv","Ps",{22,2});
        check(kjv_shift&&kjv_shift->ref==VerseRef{22,1},"source-specific title numbering");
        check(!corpus.parallel_verse("sv1917","grc-ot-fixture","Ps",{100,1}),"unverified LXX mapping");
        auto refs=corpus.coordinates("sv1917","Luke");
        check(refs.front().chapter==5&&refs.back().chapter==7,"adjacent chapter context");
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
        check(reading_introduction(old.readings[0]).find("kapitel 23, vers 1 till 6")!=std::string::npos,"spoken reference");
        const std::filesystem::path user_path(argv[2]);std::filesystem::remove(user_path);
        {UserDb user(user_path);auto s=user.load();check(s.theme==Theme::System,"default theme");s.theme=Theme::Dark;s.calendar=CalendarStyle::Old;s.parallel="en";s.font_size=24;user.save(s);}
        {UserDb user(user_path);auto s=user.load();check(s.theme==Theme::Dark&&s.calendar==CalendarStyle::Old&&s.parallel=="en"&&s.font_size==24,"persisted settings");}
        std::filesystem::remove(user_path);
        std::cout<<checks<<" checks passed.\n";return 0;
    } catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}
}
