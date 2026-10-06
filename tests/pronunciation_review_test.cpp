#include "speech/pronunciation_review.hpp"
#include "speech/speech.hpp"
#include "storage/database.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(int argc,char** argv) {
    try {
        if(argc!=3)throw std::runtime_error("Expected resources and temporary directory");
        using namespace ortho;
        const std::filesystem::path resources=argv[1],directory=argv[2];
        std::filesystem::create_directories(directory);
        const auto db_path=directory/"review.db";
        std::filesystem::remove(db_path);
        auto words=load_pronunciation_words(resources/"lexicon/sv1917-words.tsv");
        check(words.size()==28487,"prepared word list coverage");
        check(pronunciation_key("ÄR Å Ö")=="är å ö","Swedish case folding");
        check(contains_speech_word("Saul, och SAULS söner.","saul"),"word with punctuation");
        check(!contains_speech_word("Sauls söner.","Saul"),"whole word boundary");
        {
            PronunciationReviewDb review(db_path);
            review.save({"Melkisedek","corrected","Melki-sedek","long note\nwith tab\t"});
            check(make_utterance("MELKISEDEK!","sv",review.overrides()).speech_text=="Melki-sedek!","saved correction applies");
            review.save({"MELKISEDEK","approved","","original is good"});
            check(review.decisions().size()==1,"capitalized forms share a decision");
            check(make_utterance("Melkisedek!","sv",review.overrides()).speech_text=="Melkisedek!","approval removes previous correction");
            review.save({"Mose","deferred","Mo-se","revisit"});
            check(review.overrides().empty(),"deferred decision never applies");
            bool rejected=false;
            try{review.save({"Josua","corrected","",""});}catch(const std::exception&){rejected=true;}
            check(rejected&&review.decisions().size()==2,"invalid correction rolls back");
            review.save({"Åsna","corrected","å-sna","first\nsecond\tthird"});
            review.export_tsv(directory/"export.tsv");
        }
        PronunciationReviewDb reopened(db_path);
        check(reopened.decisions().size()==3,"decisions survive reopening");
        std::ifstream exported(directory/"export.tsv");std::string text((std::istreambuf_iterator<char>(exported)),{});
        check(text.find("first second third")!=std::string::npos,"TSV escapes multiline notes");
        CorpusDb corpus(resources/"corpus/corpus.db");
        const auto examples=corpus.word_examples("Mose");
        check(examples.size()==3,"three corpus examples");
        for(const auto& example:examples)check(contains_speech_word(example.verse.text,"Mose"),"examples match whole word");
        check(corpus.word_examples("wordthatdoesnotexist").empty(),"missing context");
        std::ofstream bad(directory/"bad.tsv");bad<<"Mose\tbroken\tname\t\n";bad.close();
        bool rejected=false;try{load_pronunciation_words(directory/"bad.tsv");}catch(const std::exception&){rejected=true;}
        check(rejected,"malformed word list rejected");
        std::cout<<"Pronunciation review passed: prepared data, context, case folding, persistence, corrections, rollback and export.\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
