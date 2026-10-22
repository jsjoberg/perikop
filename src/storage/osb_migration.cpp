#include "core/reading_plan.hpp"
#include "core/reading_position.hpp"
#include "storage/database.hpp"
#include "storage/legacy_progress.hpp"
#include "storage/sqlite.hpp"
#include <algorithm>
#include <map>
#include <regex>
#include <set>
#include <tuple>

namespace ortho {
namespace {
using Identity = std::tuple<std::string, std::string, VerseRef>;
struct Point {
    std::size_t section;
    std::string book;
    VerseRef ref;
};
const std::vector<ReadingPlan>& previous_plans() {
    static const std::vector<ReadingPlan> plans{
#include "storage/reading_plan_v2_parts.inc"
    };
    return plans;
}
std::vector<Point> points(const CorpusDb& corpus, const Reading& reading, bool legacy) {
    std::vector<Point> result;
    const auto parts = reading.segments();
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const auto source =
            legacy ? legacy_frame_source(corpus, "sv", parts[i].book) : std::string(osb_reference);
        for (const auto& ref : corpus.coordinates(source, parts[i].book))
            if (corpus.contains(source, parts[i], ref))
                result.push_back({i, parts[i].book, ref});
    }
    return result;
}
std::optional<ReadingPosition> convert_position(const CorpusDb& corpus, const Reading& reading,
                                                const ReadingPosition& old,
                                                const std::vector<std::size_t>& origins) {
    const auto targets = corpus.counterparts(old.source, osb_reference, old.book, old.verse);
    const auto parts = reading.segments();
    for (std::size_t i = 0; i < parts.size(); ++i)
        if (origins[i] == old.section)
            for (const auto& [book, ref] : targets)
                if (book == parts[i].book && corpus.contains(osb_reference, parts[i], ref))
                    return reading_position(corpus, reading, i, ref);
    return {};
}
struct ConvertedReading {
    Reading reading;
    std::vector<std::size_t> origins;
};
std::optional<ConvertedReading> old_reading(const CorpusDb& corpus, const std::string& key) {
    if (key.size() > 8192)
        return {};
    static const std::regex header(R"(^reading:(sv|en|el):([^:]*)(.*)$)");
    static const std::regex part(R"(:([A-Za-z0-9]+) (\d+):(\d+)([a-z]*)-(\d+):(\d+)([a-z]*))");
    std::smatch match;
    if (!std::regex_match(key, match, header))
        return {};
    const std::string language = match[1], override_source = match[2], tail = match[3];
    std::vector<Passage> passages;
    std::vector<std::size_t> origins;
    std::size_t origin = 0;
    std::size_t consumed = 0;
    try {
        for (std::sregex_iterator it(tail.begin(), tail.end(), part), end; it != end; ++it) {
            if (std::size_t(it->position()) != consumed)
                return {};
            consumed += it->length();
            const auto& m = *it;
            Passage p{
                m[1], {std::stoi(m[2]), std::stoi(m[3]), m[4]}, {std::stoi(m[5]), std::stoi(m[6]), m[7]}};
            const auto mapped = corpus.map_passage(
                override_source.empty() ? legacy_frame_source(corpus, language, p.book) : override_source,
                osb_reference, p);
            if (mapped.empty())
                return {};
            passages.insert(passages.end(), mapped.begin(), mapped.end());
            origins.insert(origins.end(), mapped.size(), origin++);
        }
    } catch (const std::exception&) {
        return {};
    }
    if (consumed != tail.size() || passages.empty())
        return {};
    Reading reading{
        ReadingKind::OldTestament, passages.front(), {passages.begin() + 1, passages.end()}, language};
    reading.reference = osb_reference;
    reading.source_override = override_source;
    return ConvertedReading{std::move(reading), std::move(origins)};
}
} // namespace

void UserDb::migrate_reading_plans(const CorpusDb& corpus) {
    migrate_legacy_plans(corpus);
    migrate_osb(corpus);
}
void UserDb::migrate_osb(const CorpusDb& corpus) {
    using storage::Statement;
    storage::Transaction transaction(db_.get());
    {
        Statement done(db_.get(), "SELECT 1 FROM settings WHERE key='osb_v1_migrated'");
        if (done.row())
            return;
    }
    std::map<std::string, std::string> complete_dates;
    {
        Statement q(db_.get(), "SELECT key,completed FROM progress");
        while (q.row())
            complete_dates.emplace(q.text(0), q.text(1));
    }
    std::string today;
    {
        Statement q(db_.get(), "SELECT date('now','localtime')");
        if (q.row())
            today = q.text(0);
    }
    std::map<Identity, std::string> read;
    std::set<std::pair<std::string, VerseRef>> checkpoints;
    for (const auto& plan : previous_plans())
        for (std::size_t i = 0; i < plan.parts.size(); ++i) {
            const auto key = "plan:" + plan.id + ":v2:" + std::to_string(i + 1);
            const auto reading = legacy_plan_reading(corpus, plan.parts[i], "sv");
            const auto verses = points(corpus, reading, true);
            const auto done = complete_dates.find(key);
            auto stop = verses.end();
            if (done == complete_dates.end()) {
                auto saved = position(key);
                if (!saved)
                    saved = position(legacy_reading_key(reading));
                if (!saved)
                    continue;
                stop = std::ranges::find_if(verses, [&](const Point& point) {
                    return point.section == saved->section && point.book == saved->book &&
                           point.ref == saved->verse &&
                           saved->source == legacy_frame_source(corpus, "sv", point.book);
                });
                if (stop == verses.end())
                    continue;
                for (const auto& target :
                     corpus.counterparts(saved->source, osb_reference, saved->book, saved->verse))
                    checkpoints.insert(target);
            }
            for (auto it = verses.begin(); it != stop; ++it) {
                const Identity id{legacy_frame_source(corpus, "sv", it->book), it->book, it->ref};
                read[id] = std::max(read[id], done == complete_dates.end() ? today : done->second);
            }
        }
    Statement mark(db_.get(), "INSERT INTO progress(key,completed) VALUES(?,?) ON CONFLICT DO NOTHING");
    for (const auto& plan : reading_plans())
        for (std::size_t i = 0; i < plan.parts.size(); ++i) {
            const auto key = plan_key(plan, i);
            if (complete_dates.contains(key) || position(key))
                continue;
            const auto reading = plan_reading(corpus, plan.parts[i], "sv");
            const auto verses = points(corpus, reading, false);
            std::optional<Point> unread;
            bool known = false;
            std::string date;
            for (const auto& point : verses) {
                const auto source = legacy_frame_source(corpus, "sv", point.book);
                const auto required = corpus.counterparts(osb_reference, source, point.book, point.ref);
                bool covered = !required.empty();
                for (const auto& [book, ref] : required) {
                    const auto found = read.find({source, book, ref});
                    if (found == read.end())
                        covered = false;
                    else {
                        known = true;
                        date = std::max(date, found->second);
                    }
                }
                known = known || checkpoints.contains({point.book, point.ref});
                if (!covered && !unread)
                    unread = point;
            }
            if (!verses.empty() && !unread) {
                mark.text(1, key);
                mark.text(2, date);
                mark.row();
                mark.reset();
            } else if (known && unread) {
                const auto saved = reading_position(corpus, reading, unread->section, unread->ref);
                if (saved) {
                    position(key, *saved);
                    const auto plain = reading_key(corpus, reading);
                    if (!position(plain))
                        position(plain, *saved);
                }
            }
        }
    // Keep all old keys and source coordinates as recovery data. Write converted copies only.
    std::vector<std::pair<std::string, ReadingPosition>> old_positions;
    {
        Statement q(db_.get(), "SELECT key FROM reading_position WHERE key LIKE 'reading:%'");
        while (q.row()) {
            const auto saved = position(q.text(0));
            if (saved)
                old_positions.emplace_back(q.text(0), *saved);
        }
    }
    for (const auto& [key, saved] : old_positions)
        if (const auto reading = old_reading(corpus, key)) {
            const auto converted = convert_position(corpus, reading->reading, saved, reading->origins);
            const auto new_key = reading_key(corpus, reading->reading);
            if (converted && !position(new_key))
                position(new_key, *converted);
        }
    storage::exec(db_.get(), "INSERT INTO settings VALUES('osb_v1_migrated','1')");
    transaction.commit();
}
} // namespace ortho
