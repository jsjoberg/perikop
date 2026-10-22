#include "core/reading_plan.hpp"
#include "core/reading_position.hpp"
#include "storage/database.hpp"
#include "storage/legacy_progress.hpp"
#include "storage/sqlite.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <tuple>

namespace ortho {
namespace {
// This entire compatibility file, its frozen ranges, and the startup call can
// be retired once migration from the original plans is no longer supported.
const std::vector<ReadingPlan>& legacy_plans() {
    static const std::vector<ReadingPlan> plans{
#include "storage/legacy_reading_plan_parts.inc"
    };
    return plans;
}
const std::vector<ReadingPlan>& v2_plans() {
    static const std::vector<ReadingPlan> plans{
#include "storage/reading_plan_v2_parts.inc"
    };
    return plans;
}
using VerseIdentity = std::tuple<std::string, std::string, VerseRef>; // source, frame book, verse
struct FramedVerse {
    std::size_t section;
    VerseIdentity identity;
};
std::vector<FramedVerse> framed_verses(const CorpusDb& corpus, const Reading& reading) {
    std::vector<FramedVerse> result;
    const auto passages = reading.segments();
    for (std::size_t section = 0; section < passages.size(); ++section) {
        const auto& passage = passages[section];
        const auto source = legacy_frame_source(corpus, "sv", passage.book);
        for (const auto& verse : corpus.coordinates(source, passage.book))
            if (passage.contains(verse))
                result.push_back({section, {source, passage.book, verse}});
    }
    return result;
}
std::optional<ReadingPosition> legacy_position(const CorpusDb& corpus, const Reading& reading,
                                               std::size_t section, VerseRef ref) {
    const auto verses = framed_verses(corpus, reading);
    for (std::size_t i = 0; i < verses.size(); ++i) {
        const auto& [source, book, verse] = verses[i].identity;
        if (verses[i].section == section && verse == ref)
            return ReadingPosition{section, book, source, ref, int(100 * i / verses.size())};
    }
    return {};
}
} // namespace

void UserDb::migrate_legacy_plans(const CorpusDb& corpus) {
    using storage::Statement;
    storage::Transaction transaction(db_.get());
    {
        Statement migrated(db_.get(), "SELECT 1 FROM settings WHERE key='reading_plans_v2_migrated'");
        if (migrated.row())
            return;
    }
    std::map<std::string, std::string> completed;
    {
        Statement query(db_.get(), "SELECT key,completed FROM progress");
        while (query.row())
            completed.emplace(query.text(0), query.text(1));
    }
    std::string today;
    {
        Statement date(db_.get(), "SELECT date('now','localtime')");
        if (date.row())
            today = date.text(0);
    }
    std::map<VerseIdentity, std::string> read;
    std::set<VerseIdentity> checkpoints;
    for (const auto& plan : legacy_plans())
        for (std::size_t i = 0; i < plan.parts.size(); ++i) {
            const auto key = "plan:" + plan.id + ":" + std::to_string(i + 1);
            const auto reading = legacy_plan_reading(corpus, plan.parts[i], "sv");
            const auto verses = framed_verses(corpus, reading);
            const auto done = completed.find(key);
            auto end = verses.end();
            if (done == completed.end()) {
                auto saved = position(key);
                if (!saved)
                    saved = position(legacy_reading_key(reading));
                if (!saved)
                    continue;
                end = std::ranges::find_if(verses, [&](const FramedVerse& verse) {
                    return verse.section == saved->section &&
                           verse.identity == VerseIdentity{saved->source, saved->book, saved->verse};
                });
                if (end == verses.end())
                    continue; // A stale checkpoint cannot prove that any text was read.
                checkpoints.insert(end->identity);
            }
            const auto& date = done == completed.end() ? today : done->second;
            for (auto verse = verses.begin(); verse != end; ++verse)
                read[verse->identity] = std::max(read[verse->identity], date);
        }
    Statement mark(db_.get(), "INSERT INTO progress(key,completed) VALUES(?,?) ON CONFLICT DO NOTHING");
    for (const auto& plan : v2_plans())
        for (std::size_t i = 0; i < plan.parts.size(); ++i) {
            const auto key = "plan:" + plan.id + ":v2:" + std::to_string(i + 1);
            if (completed.contains(key) || position(key))
                continue; // Never overwrite progress already saved in the new layout.
            const auto reading = legacy_plan_reading(corpus, plan.parts[i], "sv");
            const auto verses = framed_verses(corpus, reading);
            const auto unread = std::ranges::find_if(verses, [&](const FramedVerse& verse) {
                return !read.contains(verse.identity);
            });
            if (!verses.empty() && unread == verses.end()) {
                std::string date;
                for (const auto& verse : verses)
                    date = std::max(date, read.at(verse.identity));
                mark.text(1, key);
                mark.text(2, date);
                mark.row();
                mark.reset();
            } else if (unread != verses.end() && std::ranges::any_of(verses, [&](const FramedVerse& verse) {
                           return read.contains(verse.identity) || checkpoints.contains(verse.identity);
                       })) {
                const auto& [source, book, verse] = unread->identity;
                const auto checkpoint = legacy_position(corpus, reading, unread->section, verse);
                if (checkpoint && checkpoint->source == source && checkpoint->book == book) {
                    position(key, *checkpoint);
                    const auto plain = legacy_reading_key(reading);
                    if (!position(plain))
                        position(plain, *checkpoint);
                }
            }
        }
    // Keep original rows as recovery data. Versioned keys prevent them from
    // being interpreted as the new parts, even after this converter is removed.
    storage::exec(db_.get(), "INSERT INTO settings VALUES('reading_plans_v2_migrated','1')");
    transaction.commit();
}
} // namespace ortho
