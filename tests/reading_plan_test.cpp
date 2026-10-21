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
    check(plans.size() == 3 && plans[0].parts.size() == 60 && plans[1].parts.size() == 180 &&
              plans[2].parts.size() == 31,
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
            for (const auto& utterance :
                 reading_speech(corpus, {plan_reading(corpus, part, "sv")}, [](const std::string&) {
                     return std::vector<Pronunciation>{};
                 })) {
                std::istringstream text(utterance.display_text);
                for (std::string word; text >> word;)
                    ++words;
            }
            check(words > 0 && words < 4'800, "Every part has text and a bounded reading length");
            check(words >= 1'600 || (part.size() == 1 && part.front().book == "PrMan"),
                  "Only the complete short Prayer of Manasseh is a very short part");
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
void prefix_test(const CorpusDb& corpus, const std::filesystem::path& path) {
    const auto& nt = reading_plans().front();
    UserDb user(path);
    auto settings = user.load();
    settings.speech_voice = "alice";
    user.save(settings);
    user.complete("plan:nt:1"); // Matt 1–9
    user.complete("plan:nt:2"); // Matt 10–18
    user.complete("plan:nt:4"); // Mark 1–9, deliberately out of sequence
    user.complete("day:preserved");
    user.bookmark("sv1917", {"John", {3, 16}, {3, 16}});
    seed_date(path);
    const auto old = plan_reading(corpus, {{"Matt", 19, 28}}, "sv");
    const auto saved = reading_position(corpus, old, 0, {22, 8});
    check(saved.has_value(), "Legacy resume fixture exists");
    user.position("plan:nt:3", *saved);
    user.migrate_reading_plans(corpus);
    const auto done = user.completed();
    for (std::size_t i = 0; i < 5; ++i)
        check(done.contains(plan_key(nt, i)), "Completed chapters and a checkpoint fill earlier new parts");
    const auto current = part_at(nt, "Matt", 22);
    const auto reading = plan_reading(corpus, nt.parts[current], "sv");
    const auto expected = reading_position(corpus, reading, 0, {22, 8});
    check(!done.contains(plan_key(nt, current)) && user.position(plan_key(nt, current)) == expected &&
              user.position(reading_key(corpus, reading)) == expected,
          "The current verse and recalculated percent transfer to its new part");
    check(done.contains(plan_key(nt, part_at(nt, "Mark", 1))) &&
              done.contains(plan_key(nt, part_at(nt, "Mark", 5))) &&
              !done.contains(plan_key(nt, part_at(nt, "Mark", 8))),
          "Out-of-order completed readings transfer without marking unread chapters complete");
    check(user.position(plan_key(nt, part_at(nt, "Mark", 8)))->verse == VerseRef{10, 1},
          "A boundary part resumes at its first unread chapter");
    check(date_of(path, plan_key(nt, 0)) == "2026-10-01" && user.position("plan:nt:3") == saved &&
              done.contains("plan:nt:1") && done.contains("day:preserved") &&
              user.load().speech_voice == "alice" && user.bookmarks(corpus).size() == 1,
          "Migration preserves legacy dates, recovery data, settings, bookmarks, and day progress");
    user.migrate_reading_plans(corpus);
    check(user.completed() == done && user.position(plan_key(nt, current)) == expected,
          "Repeating the migration is a no-op");
    user.forget("plan:nt:");
    user.migrate_reading_plans(corpus);
    check(user.completed() == std::set<std::string>{"day:preserved"} && !user.position(plan_key(nt, current)),
          "Starting over clears old and new plan progress without resurrecting it");
}
void all_done_test(const CorpusDb& corpus, const std::filesystem::path& path) {
    {
        UserDb user(path);
        for (const auto& [id, count] : {std::pair{"nt", 30}, {"ot", 30}, {"lxx", 14}})
            for (int i = 1; i <= count; ++i)
                user.complete("plan:" + std::string(id) + ":" + std::to_string(i));
        seed_date(path);
        user.migrate_reading_plans(corpus);
    }
    UserDb reopened(path);
    reopened.migrate_reading_plans(corpus);
    const auto done = reopened.completed();
    for (const auto& plan : reading_plans())
        for (std::size_t i = 0; i < plan.parts.size(); ++i)
            check(done.contains(plan_key(plan, i)) && !reopened.position(plan_key(plan, i)) &&
                      date_of(path, plan_key(plan, i)) == "2026-10-01",
                  "Every completed old plan becomes a fully completed new plan, including LXX framing");
}
void checkpoint_test(const CorpusDb& corpus, const std::filesystem::path& root) {
    const auto& plans = reading_plans();
    {
        UserDb user(root / "cross-book.db");
        const auto old = plan_reading(corpus, {{"Mark", 10, 16}, {"Luke", 1, 2}}, "sv");
        user.position("plan:nt:5", *reading_position(corpus, old, 1, {1, 30}));
        user.migrate_reading_plans(corpus);
        const auto next = part_at(plans[0], "Luke", 1);
        const auto expected =
            reading_position(corpus, plan_reading(corpus, plans[0].parts[next], "sv"), 0, {1, 30});
        check(user.position(plan_key(plans[0], next)) == expected,
              "A mixed legacy part maps its second section to the new book's first section");
        const auto gap = part_at(plans[0], "Mark", 8);
        check(!user.completed().contains(plan_key(plans[0], gap)) &&
                  user.position(plan_key(plans[0], gap))->verse == VerseRef{8, 1},
              "A checkpoint cannot skip an unread gap before previously read chapters");
    }
    for (const auto& [name, part, section, verse, book, chapter] :
         {std::tuple{"nehemiah", PlanPart{{"Neh", 1, 13}, {"Esth", 1, 10}}, 0, VerseRef{17, 1}, "Neh", 7},
          std::tuple{"esther", PlanPart{{"Neh", 1, 13}, {"Esth", 1, 10}}, 1, VerseRef{2, 7}, "Esth", 2}}) {
        UserDb user(root / (std::string(name) + ".db"));
        const auto old = plan_reading(corpus, part, "sv");
        user.position("plan:ot:18", *reading_position(corpus, old, std::size_t(section), verse));
        user.migrate_reading_plans(corpus);
        const auto next = part_at(plans[1], book, chapter);
        const auto expected =
            reading_position(corpus, plan_reading(corpus, plans[1].parts[next], "sv"), 0, verse);
        check(expected && user.position(plan_key(plans[1], next)) == expected,
              "Checkpoint migration preserves LXX offsets and Greek Esther's frame book");
    }
    {
        UserDb user(root / "plain.db");
        const auto old = plan_reading(corpus, {{"Matt", 1, 9}}, "sv");
        user.position(reading_key(corpus, old), *reading_position(corpus, old, 0, {5, 4}));
        user.migrate_reading_plans(corpus);
        check(user.position(plan_key(plans[0], 0))->verse == VerseRef{5, 4},
              "A saved reading identity transfers even without a separate plan checkpoint");
    }
    {
        UserDb user(root / "stale.db");
        user.position("plan:nt:1", {99, "Matt", "sv1917", {5, 4}, 70});
        user.complete("plan:nt:7"); // The old part 7 is Luke 12–20, not the new Matt 23–25.
        user.migrate_reading_plans(corpus);
        check(!user.position(plan_key(plans[0], 0)) && !user.completed().contains(plan_key(plans[0], 6)),
              "Stale checkpoints are ignored and reused part numbers cannot corrupt progress");
    }
}
void rollback_test(const CorpusDb& corpus, const std::filesystem::path& path) {
    UserDb user(path);
    user.complete("plan:nt:1");
    user.complete("plan:nt:2");
    user.complete("plan:nt:4");
    auto db = storage::open(path, SQLITE_OPEN_READWRITE);
    storage::exec(db.get(), "CREATE TRIGGER fail_migration BEFORE INSERT ON progress "
                            "WHEN NEW.key='plan:nt:v2:9' BEGIN SELECT RAISE(ABORT,'injected failure'); END");
    bool failed = false;
    try {
        user.migrate_reading_plans(corpus);
    } catch (const std::runtime_error&) {
        failed = true;
    }
    storage::Statement marker(db.get(), "SELECT 1 FROM settings WHERE key='reading_plans_v2_migrated'");
    check(failed && !marker.row() &&
              user.completed() == std::set<std::string>{"plan:nt:1", "plan:nt:2", "plan:nt:4"} &&
              !user.position(plan_key(reading_plans()[0], 4)),
          "A failed migration rolls back all new progress and its completion marker");
    storage::exec(db.get(), "DROP TRIGGER fail_migration");
    user.migrate_reading_plans(corpus);
    check(user.completed().contains(plan_key(reading_plans()[0], 0)),
          "A failed migration can retry safely on the next startup");
}
void existing_test(const CorpusDb& corpus, const std::filesystem::path& root) {
    const auto& nt = reading_plans().front();
    {
        UserDb user(root / "empty.db");
        user.migrate_reading_plans(corpus);
        user.migrate_reading_plans(corpus);
        check(user.completed().empty() && !user.position(plan_key(nt, 0)),
              "A new profile starts with no fabricated progress");
    }
    {
        UserDb user(root / "existing.db");
        user.complete("plan:nt:1");
        const auto current = reading_position(corpus, plan_reading(corpus, nt.parts[0], "sv"), 0, {3, 5});
        user.position(plan_key(nt, 0), *current);
        user.complete(plan_key(nt, 1));
        user.migrate_reading_plans(corpus);
        check(user.position(plan_key(nt, 0)) == current && !user.completed().contains(plan_key(nt, 0)) &&
                  user.completed().contains(plan_key(nt, 1)),
              "Existing v2 checkpoints and completion marks take precedence over legacy data");
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
        prefix_test(corpus, root / "prefix.db");
        all_done_test(corpus, root / "complete.db");
        checkpoint_test(corpus, root);
        existing_test(corpus, root);
        rollback_test(corpus, root / "rollback.db");
        std::filesystem::remove_all(root);
        std::cout << "Reading-plan layout and legacy migration passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
