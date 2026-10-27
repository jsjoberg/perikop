#include "core/source_preferences.hpp"
#include "speech/reading_speech.hpp"
#include "storage/content_import.hpp"
#include "storage/database.hpp"
#include "storage/sqlite.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <picosha2.h>
#include <stdexcept>

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
            check(corpus.has_imported() && corpus.read_only(), "Local import changed base corpus policy");
            check(corpus.sources().size() == 6, "Combined edition is not available");
            const auto john = corpus.verse(ortho::srb_source, "John", {3, 16});
            check(john.has_value(), "Imported John 3:16 is missing");
            const auto colossians = corpus.verse(ortho::srb_source, "Col", {2, 1});
            const auto jude = corpus.verse(ortho::srb_source, "Jude", {1, 1});
            check(colossians && colossians->text.starts_with("Jag ") && jude &&
                      jude->text.starts_with("Judas, "),
                  "Times New Roman chapter drop capitals were discarded");
            const auto legacy = temporary / "legacy-srb.db";
            std::filesystem::copy_file(destination, legacy);
            {
                auto db = ortho::storage::open(legacy, SQLITE_OPEN_READWRITE);
                ortho::storage::exec(
                    db.get(), "UPDATE imported_verse SET text=substr(text,2) WHERE verse=1 AND "
                              "((book='Col' AND chapter=2) OR (book='Jude' AND chapter=1));"
                              "UPDATE content_metadata SET "
                              "value='06e9957d4b860bfdfc9e70b24df6f23c2332f19314e45c9d0cd88ae5035fc625' "
                              "WHERE key='srb2026'");
            }
            const auto legacy_hash = hash_file(legacy);
            ortho::CorpusDb previous(corpus_file);
            previous.load_imported(legacy);
            check(previous.verse(ortho::srb_source, "Col", {2, 1})->text == colossians->text &&
                      previous.verse(ortho::srb_source, "Jude", {1, 1})->text == jude->text &&
                      hash_file(legacy) == legacy_hash,
                  "Existing 0.2.0 imports must recover their initials without modifying local data");
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
