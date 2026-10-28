#include "core/source_preferences.hpp"
#include "speech/reading_speech.hpp"
#include "storage/content_import.hpp"
#include "storage/database.hpp"
#include "storage/sqlite.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <picosha2.h>
#include <stdexcept>
#include <tuple>

namespace {
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
std::string hash_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    const std::string value{std::istreambuf_iterator<char>(in), {}};
    return picosha2::hash256_hex_string(value);
}
template <typename F> void rejected(F action) {
    bool failed = false;
    try {
        action();
    } catch (const std::exception&) {
        failed = true;
    }
    check(failed, "Invalid import was accepted");
}
} // namespace
int main(int argc, char** argv) {
    try {
        check(argc == 4 || argc == 5,
              "Expected corpus, recipe, temporary directory, and optional local PDF directory");
        const std::filesystem::path corpus_file = argv[1], recipe = argv[2], temporary = argv[3];
        std::filesystem::remove_all(temporary);
        std::filesystem::create_directories(temporary);
        const auto invalid_pdf = temporary / "invalid.pdf", destination = temporary / "content/srb2026.db";
        std::ofstream(invalid_pdf, std::ios::binary) << "%PDF-1.6 test data only";
        rejected([&] {
            ortho::import_reformationsbibeln(invalid_pdf, invalid_pdf, recipe, destination);
        });
        check(!std::filesystem::exists(destination), "Rejected PDFs created an installed database");
        if (argc == 5) {
            const std::filesystem::path directory = argv[4];
            const auto gt = directory / "SRBhelaGT.pdf", nt = directory / "SRBNT-UNnoter.pdf";
            const auto original = hash_file(corpus_file);
            ortho::CorpusDb corpus(corpus_file);
            check(corpus.coordinates(ortho::srb_source, "John").empty(), "SRB is bundled before import");
            int last_progress = -1;
            const auto result = ortho::import_reformationsbibeln(gt, nt, recipe, destination, [&](int value) {
                check(value >= last_progress && value <= 100, "Import progress regressed");
                last_progress = value;
            });
            check(result.books == 66 && result.verses == 31171 && last_progress == 100, "Incomplete import");
            corpus.load_imported(destination);
            {
                auto db = ortho::storage::open(destination, SQLITE_OPEN_READONLY);
                ortho::storage::Statement headings(db.get(),
                                                   "SELECT text FROM imported_heading ORDER BY ordinal");
                std::string complete;
                int count = 0;
                while (headings.row()) {
                    complete += headings.text(0) + '\n';
                    ++count;
                }
                check(count == 1184 && picosha2::hash256_hex_string(complete) ==
                                           "a82b6db6a76b9a583d393c97c0249ba1879ec599435044dfb4ded46efeb489df",
                      "Audited heading text changed or headings are missing");
            }
            const auto psalm_opening = corpus.verse(ortho::srb_source, "Ps", {119, 1});
            check(psalm_opening && psalm_opening->headings.size() == 2,
                  "Psalm 119 lost its chapter summary or first alphabet heading");
            for (int verse = 9; verse <= 169; verse += 8) {
                const auto section = corpus.verse(ortho::srb_source, "Ps", {119, verse});
                const auto preceding = corpus.verse(ortho::srb_source, "Ps", {119, verse - 1});
                check(section && section->headings.size() == 1 && preceding && preceding->headings.empty(),
                      "Alphabet heading was lost or attached to the preceding verse");
            }
            const auto mapped_heading = corpus.parallel_verse("grc-lxx", ortho::srb_source, "Ps", {118, 9});
            check(mapped_heading &&
                      mapped_heading->headings == corpus.verse(ortho::srb_source, "Ps", {119, 9})->headings,
                  "Parallel numbering dropped a section heading");
            for (const auto* book : {"Gen", "Matt", "Mark", "Luke", "John"})
                check(!corpus.verse(ortho::srb_source, book, {1, 1})->headings.empty(),
                      "Chapter summaries are missing from the heading metadata");
            check(corpus.has_imported() && corpus.read_only(), "Local import changed base corpus policy");
            check(corpus.sources().size() == 6, "Combined edition is not available");
            const auto john = corpus.verse(ortho::srb_source, "John", {3, 16});
            check(john.has_value(), "Imported John 3:16 is missing");
            const auto colossians = corpus.verse(ortho::srb_source, "Col", {2, 1});
            const auto jude = corpus.verse(ortho::srb_source, "Jude", {1, 1});
            check(colossians && colossians->text.starts_with("Jag ") && jude &&
                      jude->text.starts_with("Judas, "),
                  "Times New Roman chapter drop capitals were discarded");
            const std::array<std::tuple<const char*, int, int, const char*>, 15> verified_verses{{
                {"Lev", 11, 1, "379692a2d4863312ef527878345fe3bb45e8f965ce9eaeb3be65ba7c70da0d80"},
                {"Exod", 15, 7, "f9e1fe691b91bb3a1d820aab830ae2b68be547f36432aa7543baa421a721413c"},
                {"Exod", 24, 3, "dd83ee607ff5593aa3dae9fcae2ba540d465f234baee412cf53126c464d2d67c"},
                {"Luke", 13, 3, "585973c2bd136216328f5f2f1c60ab3b7fdac0de671552911d21082c3c5eaa68"},
                {"Luke", 13, 5, "3da9ac3d5e03003438a43fda256a47f912f65e532cf3e98662386cedc7b45308"},
                {"1Chr", 1, 1, "b02d1bb46c32d96dc64604df93b96bb968eb8af3544e44f50308bdcc29015afc"},
                {"Ps", 119, 8, "94c88ffd05166e9504f01be3dddc1d553e4ea758bc693595810fa10d5ff850fe"},
                {"2Chr", 2, 14, "e34b2168232918a83e8a928d450b751bdd7dd9d4e3b50d724950f997791a8fa2"},
                {"Josh", 18, 28, "7834c6e5fcd1e89635e143dda706171e2bc65d490bf775c29485ca28ddff7eb7"},
                {"Gen", 37, 28, "b30c3d3eb74f5ab8df31ec2b917f0e6b320128b823034ee937fc92fda7cbb35a"},
                {"Num", 33, 9, "1f30030407ebe749739b7696d1559f42a067775e2fa833b429e2a1097fca1b3c"},
                {"Jer", 29, 17, "fcec882709fa1144883f3986f75a8d5dda21bfb26fc53ef46c157820dad11cc4"},
                {"Rev", 11, 3, "846fb87377f54391c822f162f8cca73ea46f62681b4d797352ae329fc00f3797"},
                {"Rev", 16, 21, "63cd22f8f90270c986ca62d9f0b0bde5a9243699af7ad74b259b25d75414e02b"},
                {"Rom", 6, 5, "74244b6da577663c14a2941e5f6ccbc3cb4d8798472fe25bd8aaa0ddf8b8e683"},
            }};
            for (const auto& [book, chapter, verse, hash] : verified_verses) {
                const auto value = corpus.verse(ortho::srb_source, book, {chapter, verse});
                check(value && picosha2::hash256_hex_string(value->text) == hash,
                      "Audited Scripture changed, or chapter furniture leaked into a verse");
            }
            // Existing imports are read as stored; only an explicit reimport replaces their data.
            const auto existing = temporary / "previous-srb.db";
            std::filesystem::copy_file(destination, existing);
            {
                auto db = ortho::storage::open(existing, SQLITE_OPEN_READWRITE);
                ortho::storage::exec(db.get(), "DROP TABLE imported_heading;"
                                               "UPDATE imported_verse SET text='Unchanged local text' WHERE "
                                               "book='Col' AND chapter=2 AND verse=1");
            }
            const auto existing_hash = hash_file(existing);
            ortho::CorpusDb previous(corpus_file);
            previous.load_imported(existing);
            const auto stored = previous.verse(ortho::srb_source, "Col", {2, 1});
            check(stored && stored->text == "Unchanged local text" &&
                      previous.verse(ortho::srb_source, "Ps", {119, 1})->headings.empty() &&
                      hash_file(existing) == existing_hash,
                  "Reading an existing import modified its text or database");
            ortho::import_reformationsbibeln(gt, nt, recipe, existing);
            previous.load_imported(existing);
            int checked = 0;
            for (const auto& book : corpus.source_books(ortho::srb_source))
                for (const auto& ref : corpus.coordinates(ortho::srb_source, book)) {
                    const auto expected = corpus.verse(ortho::srb_source, book, ref);
                    const auto actual = previous.verse(ortho::srb_source, book, ref);
                    check(actual && expected && actual->text == expected->text &&
                              actual->headings == expected->headings,
                          "Explicit reimport did not replace the stored text and headings");
                    ++checked;
                }
            check(checked == 31171, "Reimport missed verses");
            const auto genesis = corpus.verse(ortho::srb_source, "Gen", {1, 1});
            check(genesis && picosha2::hash256_hex_string(genesis->text) ==
                                 "0bd7cd4901c1c4a628564c789efc1b0aa0c3dc6e1c9f57973853712c9dcbe235",
                  "Note-marker removal lost a word boundary");
            check(picosha2::hash256_hex_string(john->text) ==
                      "15ed6d6f7327773d8c95b36f821349358195a4901dc388760e900e6108ddd77b",
                  "John 3:16 extraction changed");
            const auto available = ortho::chapter_sources(corpus, "sv1917", "John", 3);
            check(std::ranges::find(available, ortho::srb_source) != available.end(),
                  "Imported edition is not selectable");
            const auto psalm = corpus.parallel_verse("grc-lxx", ortho::srb_source, "Ps", {22, 1});
            check(psalm && psalm->ref.chapter == 23, "Psalm mapping uses the wrong numbering");
            const auto john_end =
                corpus.parallel_verse("grc-patriarchal", ortho::srb_source, "John", {1, 52});
            check(john_end && john_end->ref == ortho::VerseRef{1, 51}, "John's numbering shifted");
            const auto corinthians =
                corpus.parallel_verse("grc-patriarchal", ortho::srb_source, "2Cor", {13, 13});
            check(corinthians && corinthians->ref == ortho::VerseRef{13, 13}, "Merged letter ending shifted");
            const auto joshua = corpus.verse(ortho::srb_source, "Josh", {21, 36});
            check(joshua.has_value(), "Distinct SRB verse was discarded");
            check(corpus.verse(ortho::srb_source, "Micah", {1, 1}).has_value() &&
                      corpus.verse(ortho::srb_source, "Philemon", {1, 25}).has_value(),
                  "Canonical book codes changed");
            check(!corpus.saldo("kungen").empty() && !corpus.saldo("ska").empty(),
                  "Modern word study is missing");
            ortho::Reading reading{ortho::ReadingKind::Gospel, {"John", {3, 16}, {3, 16}}};
            reading.source_override = ortho::srb_source;
            reading.reference = ortho::srb_source;
            const auto speech = ortho::reading_speech(corpus, {reading}, [&](const auto& language) {
                return corpus.pronunciations(language);
            });
            check(speech.size() == 1 && speech.front().display_text == john->text &&
                      speech.front().language == "sv",
                  "Speech reads a different edition");
            // A failed replacement leaves the installed text and mappings intact.
            {
                auto db = ortho::storage::open(destination, SQLITE_OPEN_READWRITE);
                ortho::storage::exec(db.get(),
                                     "CREATE TRIGGER fail_replacement BEFORE INSERT ON imported_verse BEGIN "
                                     "SELECT RAISE(ABORT,'test rollback'); END");
            }
            rejected([&] {
                ortho::import_reformationsbibeln(gt, nt, recipe, destination);
            });
            ortho::CorpusDb reopened(corpus_file);
            reopened.load_imported(destination);
            const auto preserved = reopened.verse(ortho::srb_source, "John", {3, 16});
            check(preserved && preserved->text == john->text, "Failed import lost the existing edition");
            check(reopened.verse(ortho::srb_source, "Ps", {119, 1})->headings == psalm_opening->headings,
                  "Failed import lost the existing headings");
            {
                // Without the failing trigger, so that only the swapped files can reject the import.
                auto db = ortho::storage::open(destination, SQLITE_OPEN_READWRITE);
                ortho::storage::exec(db.get(), "DROP TRIGGER fail_replacement");
            }
            rejected([&] {
                ortho::import_reformationsbibeln(nt, gt, recipe, destination);
            });
            check(hash_file(corpus_file) == original, "Bundled corpus was modified");
        }
        std::cout << "Content import checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
