#include "core/reading_display.hpp"
#include "core/reading_plan.hpp"
#include "core/reading_position.hpp"
#include "speech/reading_speech.hpp"
#include "storage/database.hpp"
#include "storage/sqlite.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>

using namespace ortho;
namespace {
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void counterpart(const CorpusDb& corpus, const char* source, const char* book, VerseRef ref,
                 const char* target_book, VerseRef target) {
    const auto found = corpus.counterparts(osb_reference, source, book, ref);
    check(std::ranges::find(found, std::pair{std::string(target_book), target}) != found.end(),
          "Reviewed OSB crosswalk changed");
}
} // namespace
int main(int argc, char** argv) {
    try {
        check(argc == 3, "Expected corpus and temporary profile");
        CorpusDb corpus(argv[1]);
        const auto sources = corpus.sources();
        check(std::ranges::none_of(sources,
                                   [](const auto& source) {
                                       return source.code == osb_reference;
                                   }),
              "The reference authority is not a selectable text edition");
        check(!corpus.verse(osb_reference, "John", {3, 16}), "OSB wording must not be bundled as Bible text");
        std::size_t nt = 0;
        for (const auto& book : corpus.canon()) {
            if (book.new_testament)
                nt += corpus.coordinates(osb_reference, book.code).size();
        }
        check(nt == 7957, "OSB NT has the verified complete KJV coordinate inventory");
        const auto& esther = corpus.coordinates(osb_reference, "Esth");
        check(esther.front() == VerseRef{1, 1, "a"} && esther[17] == VerseRef{1, 1, "r"} &&
                  esther[18] == VerseRef{1, 1},
              "Esther's lettered prelude precedes numeric verse 1");
        check(corpus.contains(osb_reference, {"Esth", {1, 1, "a"}, {1, 1}}, {1, 1, "r"}) &&
                  !corpus.contains(osb_reference, {"Esth", {1, 1}, {1, 2}}, {1, 1, "r"}),
              "Passage containment uses printed order rather than lexical suffix order");
        counterpart(corpus, "grc-lxx", "1Sam", {17, 12}, "1Sam", {17, 32});
        counterpart(corpus, "grc-lxx", "Prov", {30, 1}, "Prov", {24, 22, "f"});
        counterpart(corpus, "grc-lxx", "Sir", {31, 1}, "Sir", {34, 1});
        counterpart(corpus, "grc-lxx", "Neh", {4, 1}, "Ezra", {14, 1});
        counterpart(corpus, "sv1917", "Neh", {4, 1}, "Neh", {4, 7});
        counterpart(corpus, "sv1917", "2Chr", {37, 1}, "PrMan", {1, 1});
        counterpart(corpus, "sv1917", "John", {11, 36}, "John", {11, 35});
        counterpart(corpus, "sv1917", "2Cor", {13, 14}, "2Cor", {13, 13});
        counterpart(corpus, "grc-patriarchal", "3John", {1, 14}, "3John", {1, 15});
        check(corpus.counterparts(osb_reference, "grc-lxx", "John", {1, 1}).empty(),
              "Missing mappings cannot fall back to a guessed same-number lookup");
        Reading outside{ReadingKind::OldTestament, {"4Macc", {8, 29}, {8, 29}}};
        outside.reference = "en-lxx2012";
        bool rejected = false;
        try {
            corpus.localize(outside);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        check(rejected, "A source book absent from OSB cannot receive invented master coordinates");
        outside.passage = {"Gen", {1, 1}, {1, 1}};
        outside.additional = {{"4Macc", {8, 29}, {8, 29}}};
        rejected = false;
        try {
            corpus.localize(outside);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        check(rejected, "An unmapped section cannot silently disappear from a composite reading");
        Reading old{ReadingKind::Gospel, {"John", {11, 35}, {11, 35}}};
        old.reference = "sv1917";
        const auto localized = corpus.localize(old);
        check(localized.reference == osb_reference && localized.passage.first == VerseRef{11, 36} &&
                  corpus.localize(localized).passage.first == localized.passage.first,
              "Localization records its authority and is idempotent");
        Reading ending{ReadingKind::Epistle, {"2Cor", {13, 12}, {13, 14}}};
        ending.reference = osb_reference;
        const auto speech = reading_speech(corpus, {ending}, [](const auto&) {
            return std::vector<Pronunciation>{};
        });
        check(speech.size() == 2 && speech[0].cue->verse == VerseRef{13, 12} &&
                  speech[0].cue->last == VerseRef{13, 13} && speech[1].cue->verse == VerseRef{13, 14} &&
                  speech[1].display_text == corpus.verse("sv1917", "2Cor", {13, 13})->text,
              "Merged verses render and speak the same source words once, with OSB cues");
        Reading inside_merge{ReadingKind::Epistle, {"2Cor", {13, 13}, {13, 13}}};
        const auto selected = reading_speech(corpus, {inside_merge}, [](const auto&) {
            return std::vector<Pronunciation>{};
        });
        check(selected.size() == 1 &&
                  selected[0].display_text == corpus.verse("sv1917", "2Cor", {13, 12})->text &&
                  selected[0].cue->verse == VerseRef{13, 13},
              "A selection starting inside a merged verse still reads its complete translation text");
        // Validate the entire generated graph, including inverse links and real source coordinates.
        std::size_t links = 0;
        for (const auto& source : sources)
            for (const auto& book : corpus.canon())
                for (const auto& ref : corpus.coordinates(osb_reference, book.code))
                    for (const auto& [target_book, target] :
                         corpus.counterparts(osb_reference, source.code, book.code, ref)) {
                        check(corpus.verse(source.code, target_book, target).has_value(),
                              "OSB link points to absent source text");
                        const auto reverse =
                            corpus.counterparts(source.code, osb_reference, target_book, target);
                        check(std::ranges::find(reverse, std::pair{book.code, ref}) != reverse.end(),
                              "OSB crosswalk lost its inverse");
                        ++links;
                    }
        check(links > 100000, "Full edition crosswalks must remain present");
        for (const auto& rule : corpus.reading_rules())
            if (rule.reading.can_open())
                check(corpus.localize(rule.reading).can_open(),
                      "A lectionary assignment lost its OSB mapping");
        const std::filesystem::path profile = argv[2];
        std::filesystem::remove(profile);
        {
            UserDb user(profile);
            user.bookmark("grc-lxx", {"Ezra", {11, 1}, {11, 3}});
            user.bookmark(osb_reference, {"Esth", {1, 1, "a"}, {1, 1}});
            const auto marks = user.bookmarks(corpus);
            check(marks.size() == 2 && marks[0].source == "grc-lxx" && marks[0].passage.first.chapter == 11 &&
                      passage_label(corpus, bookmark_passages(corpus, marks[0])) == "Nehemja 1:1–3",
                  "Old bookmarks retain their original data and display OSB references");
        }
        std::filesystem::remove(profile);
        std::cout << "OSB references, source mappings, speech, bookmarks and " << links
                  << " inverse links passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
