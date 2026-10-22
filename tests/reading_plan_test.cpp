#include "core/reading_plan.hpp"
#include "core/reading_position.hpp"
#include "speech/reading_speech.hpp"
#include "storage/database.hpp"
#include "storage/sqlite.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>

using namespace ortho;
namespace {
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
std::size_t part_at(const ReadingPlan& plan, const std::string& book, int chapter) {
    for (std::size_t i = 0; i < plan.parts.size(); ++i)
        for (const auto& range : plan.parts[i])
            if (range.book == book && range.first <= chapter && chapter <= range.last)
                return i;
    throw std::runtime_error("Missing chapter in plan: " + book);
}
void layout_test(const CorpusDb& corpus) {
    const auto& plans = reading_plans();
    check(plans.size() == 3 && plans[0].parts.size() == 60 && plans[1].parts.size() == 182 &&
              plans[2].parts.size() == 36,
          "NT has 60 parts; other plans choose comparable reading lengths");
    const std::set<std::string> separate{"Matt", "Mark", "Luke", "John", "Acts", "Rev"};
    const std::set<std::string> long_letters{"Rom", "1Cor", "2Cor", "Heb"};
    std::map<std::string, int> appearances;
    std::vector<double> means;
    for (const auto& plan : plans) {
        std::vector<int> lengths;
        for (const auto& part : plan.parts) {
            for (const auto& range : part) {
                ++appearances[range.book];
                if (separate.contains(range.book))
                    check(part.size() == 1, "Gospels, Acts, and Revelation never share a part");
                if (part.size() > 1) {
                    const auto book = std::ranges::find(corpus.canon(), range.book, &CanonBook::code);
                    check(book != corpus.canon().end(), "Every planned book belongs to the canon");
                    int last = 0;
                    for (const auto& verse :
                         corpus.coordinates(corpus.frame_source("sv", book->frame_book), book->frame_book))
                        if (book->first_chapter <= verse.chapter && verse.chapter <= book->last_chapter)
                            last = std::max(last, verse.chapter - book->offset());
                    check(range.first == 1 && range.last == last,
                          "A mixed part contains only complete books, never the start of another book");
                }
            }
            int words = 0;
            std::optional<std::pair<std::string, std::vector<std::pair<std::string, VerseRef>>>> previous;
            const auto reading = plan_reading(corpus, part, "sv");
            for (const auto& passage : reading.segments())
                for (const auto& ref : corpus.coordinates(osb_reference, passage.book)) {
                    if (!corpus.contains(osb_reference, passage, ref))
                        continue;
                    auto source = std::string("sv1917");
                    auto targets = corpus.counterparts(osb_reference, source, passage.book, ref);
                    if (targets.empty()) {
                        source = corpus.new_testament_book(passage.book) ? "en-web" : "en-lxx2012";
                        targets = corpus.counterparts(osb_reference, source, passage.book, ref);
                    }
                    const auto unit = std::pair{source, targets};
                    if (previous && *previous == unit)
                        continue;
                    previous = unit;
                    for (const auto& [book, target] : targets)
                        if (const auto verse = corpus.verse(source, book, target)) {
                            std::istringstream text(verse->text);
                            for (std::string word; text >> word;)
                                ++words;
                        }
                }
            check(words > 0 && words < 5'200, "Every part has text and a bounded reading length");
            check(words >= 1'200, "OSB parts have comparable reading lengths");
            lengths.push_back(words);
        }
        const auto mean = double(std::accumulate(lengths.begin(), lengths.end(), 0)) / lengths.size();
        means.push_back(mean);
        std::cout << plan.id << ": " << lengths.size() << " parts, " << mean << " mean words\n";
    }
    check(std::abs(means[1] / means[0] - 1) < 0.15 && std::abs(means[2] / means[0] - 1) < 0.15,
          "The three plans have similar average reading lengths");
    for (const auto& range : plans[0].parts)
        for (const auto& book : range)
            if (!separate.contains(book.book) && !long_letters.contains(book.book))
                check(appearances[book.book] == 1, "Short NT letters remain whole");
}
void seed_date(const std::filesystem::path& path) {
    auto db = storage::open(path, SQLITE_OPEN_READWRITE);
    storage::exec(db.get(), "UPDATE progress SET completed='2026-10-01'");
}
std::string date_of(const std::filesystem::path& path, const std::string& key) {
    auto db = storage::open(path, SQLITE_OPEN_READONLY);
    storage::Statement query(db.get(), "SELECT completed FROM progress WHERE key=?");
    query.text(1, key);
    return query.row() ? query.text(0) : "";
}
void migration_tests(const CorpusDb& corpus, const std::filesystem::path& root) {
    const auto& nt = reading_plans()[0];
    {
        UserDb user(root / "v1.db");
        user.complete("plan:nt:1"); // Frozen v1: Matthew 1–9.
        user.position("plan:nt:2", {0, "Matt", "sv1917", {12, 5}, 30});
        user.complete("day:preserved");
        user.bookmark("grc-lxx", {"Ezra", {11, 1}, {11, 3}});
        seed_date(root / "v1.db");
        user.migrate_reading_plans(corpus);
        const auto key = plan_key(nt, part_at(nt, "Matt", 12));
        const auto reading = plan_reading(corpus, nt.parts[part_at(nt, "Matt", 12)], "sv");
        const auto expected = reading_position(corpus, reading, 0, {12, 5});
        check(user.completed().contains(plan_key(nt, 0)) && user.completed().contains(plan_key(nt, 1)),
              "Frozen v1 completion transfers through v2 to the OSB layout");
        check(user.position(key) == expected, "Legacy checkpoint resumes at the same words in OSB");
        check(date_of(root / "v1.db", plan_key(nt, 0)) == "2026-10-01" &&
                  user.bookmarks(corpus).size() == 1 && user.completed().contains("plan:nt:1") &&
                  user.completed().contains("day:preserved"),
              "Migration preserves dates, old rows, bookmarks and unrelated progress");
        const auto done = user.completed();
        user.migrate_reading_plans(corpus);
        check(user.completed() == done && user.position(key) == expected, "OSB migration is idempotent");
        user.forget("plan:nt:");
        user.migrate_reading_plans(corpus);
        check(!user.completed().contains(plan_key(nt, 0)) && !user.position(key),
              "Reset cannot resurrect old progress");
    }
    {
        const auto path = root / "v2.db";
        UserDb user(path);
        user.complete("plan:nt:v2:1");
        // Old Swedish John 11:35 is OSB/NKJV John 11:36.
        const auto old_key = std::string("reading:sv::John 11:1-11:57");
        const ReadingPosition saved{0, "John", "sv1917", {11, 35}, 60};
        user.position(old_key, saved);
        user.position("reading:sv::Ezra 11:1-23:31", {0, "Ezra", "grc-lxx", {17, 1}, 50});
        user.position("reading:sv::John 1:1-1:10:John 1:1-1:10", {1, "John", "sv1917", {1, 4}, 60});
        user.position("plan:nt:v2:2", {99, "Matt", "sv1917", {6, 2}, 40});
        user.migrate_reading_plans(corpus);
        Reading reading{ReadingKind::Gospel, {"John", {11, 1}, {11, 57}}};
        reading.reference = osb_reference;
        const auto converted = user.position(reading_key(corpus, reading));
        check(converted && converted->source == osb_reference && converted->verse == VerseRef{11, 36},
              "Ordinary resume keys and changed NT verse divisions migrate together");
        check(user.position(old_key) == saved, "Original resume coordinates remain as recovery data");
        Reading nehemiah{ReadingKind::OldTestament, {"Neh", {1, 1}, {13, 31}}};
        const auto neh = user.position(reading_key(corpus, nehemiah));
        check(neh && neh->book == "Neh" && neh->source == osb_reference && neh->verse == VerseRef{7, 1},
              "Combined Greek Ezra resume positions migrate into the separate OSB Nehemiah book");
        Reading repeated{ReadingKind::Gospel, {"John", {1, 1}, {1, 10}}, {{"John", {1, 1}, {1, 10}}}};
        const auto repeated_position = user.position(reading_key(corpus, repeated));
        check(repeated_position && repeated_position->section == 1 &&
                  repeated_position->verse == VerseRef{1, 4},
              "A repeated composite passage retains the current occurrence, not an earlier section");
        check(user.completed().contains(plan_key(nt, 0)) && !user.position(plan_key(nt, 1)),
              "V2 completion transfers, while stale checkpoint sections prove nothing");
    }
    {
        UserDb user(root / "priority.db");
        user.complete("plan:nt:v2:1");
        const auto current = reading_position(corpus, plan_reading(corpus, nt.parts[0], "sv"), 0, {3, 5});
        user.position(plan_key(nt, 0), *current);
        user.migrate_reading_plans(corpus);
        check(user.position(plan_key(nt, 0)) == current && !user.completed().contains(plan_key(nt, 0)),
              "Existing OSB progress wins over older completion data");
    }
    {
        static const std::vector<ReadingPlan> previous{
#include "storage/reading_plan_v2_parts.inc"
        };
        const auto& ot = reading_plans()[1];
        const auto part = part_at(ot, "2Chr", 37);
        for (bool manasseh : {false, true}) {
            UserDb user(root / (manasseh ? "manasseh-read.db" : "manasseh-unread.db"));
            for (const auto& plan : previous)
                for (std::size_t i = 0; i < plan.parts.size(); ++i)
                    if (std::ranges::any_of(plan.parts[i], [&](const auto& range) {
                            return range.book == "2Chr" || (manasseh && range.book == "PrMan");
                        }))
                        user.complete("plan:" + plan.id + ":v2:" + std::to_string(i + 1));
            user.migrate_reading_plans(corpus);
            if (manasseh)
                check(user.completed().contains(plan_key(ot, part)),
                      "Manasseh completion transfers from the other-books plan into OSB Chronicles 37");
            else {
                const auto position = user.position(plan_key(ot, part));
                check(!user.completed().contains(plan_key(ot, part)) && position &&
                          position->book == "2Chr" && position->verse == VerseRef{37, 1},
                      "Completed Chronicles cannot mark the newly attached unread Prayer complete");
            }
        }
    }
    {
        UserDb user(root / "empty.db");
        user.migrate_reading_plans(corpus);
        check(user.completed().empty() && !user.position(plan_key(nt, 0)),
              "A new profile has no invented progress");
    }
    {
        const auto path = root / "rollback.db";
        UserDb user(path);
        user.complete("plan:nt:v2:1");
        auto db = storage::open(path, SQLITE_OPEN_READWRITE);
        storage::exec(db.get(), "CREATE TRIGGER fail_osb BEFORE INSERT ON progress WHEN "
                                "NEW.key='plan:nt:v3:1' BEGIN SELECT RAISE(ABORT,'rollback test'); END");
        bool failed = false;
        try {
            user.migrate_reading_plans(corpus);
        } catch (const std::runtime_error&) {
            failed = true;
        }
        storage::Statement marker(db.get(), "SELECT 1 FROM settings WHERE key='osb_v1_migrated'");
        check(failed && !marker.row() && !user.completed().contains(plan_key(nt, 0)),
              "An interrupted OSB migration rolls back converted progress and its marker");
        storage::exec(db.get(), "DROP TRIGGER fail_osb");
        user.migrate_reading_plans(corpus);
        check(user.completed().contains(plan_key(nt, 0)),
              "Failed migration can retry without losing recovery data");
    }
}
} // namespace
int main(int argc, char** argv) {
    try {
        check(argc == 3, "Expected corpus and isolated test directory");
        CorpusDb corpus(argv[1]);
        const std::filesystem::path root(argv[2]);
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
        layout_test(corpus);
        migration_tests(corpus, root);
        std::filesystem::remove_all(root);
        std::cout << "OSB plan layout and versioned progress migration passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
