#include "speech/speech.hpp"
#include "storage/database.hpp"
#include "storage/sqlite.hpp"
#include <algorithm>
#include <map>
#include <stdexcept>
namespace ortho {
using storage::exec;
using storage::open;
using storage::pragma_number;
using storage::Statement;
using storage::Transaction;
CorpusDb::CorpusDb(const std::filesystem::path& path) : db_(open(path, SQLITE_OPEN_READONLY)) {
    exec(db_.get(), "PRAGMA query_only=ON; PRAGMA foreign_keys=ON; PRAGMA cache_size=-8192");
    if (pragma_number(db_.get(), "PRAGMA user_version") != 3 ||
        pragma_number(db_.get(), "PRAGMA application_id") != 0x4f525443)
        throw std::runtime_error("Unsupported corpus schema");
}
bool CorpusDb::read_only() const {
    return sqlite3_db_readonly(db_.get(), "main") == 1;
}
std::vector<WordExample> CorpusDb::word_examples(const std::string& word, int limit) const {
    Statement q(db_.get(), "SELECT book.code,chapter,verse,verse_suffix,text FROM verse JOIN book ON "
                           "book.id=book_id JOIN source ON source.id=source_id WHERE source.code='sv1917' "
                           "ORDER BY book.canonical_order,chapter,verse,verse_suffix");
    std::vector<WordExample> result;
    while (q.row() && int(result.size()) < limit)
        if (contains_speech_word(q.text(4), word)) {
            auto value = verse("sv1917", q.text(0), {q.number(1), q.number(2), q.text(3)});
            if (value)
                result.push_back({q.text(0), *value});
        }
    return result;
}
std::vector<Source> CorpusDb::sources() const {
    Statement query(db_.get(), "SELECT id,code,language,name,versification FROM source ORDER BY id");
    std::vector<Source> result;
    while (query.row())
        result.push_back({query.number(0), query.text(1), query.text(2), query.text(3), query.text(4)});
    return result;
}
std::expected<Verse, std::string> CorpusDb::verse(const std::string& source, const std::string& book,
                                                  VerseRef ref) const {
    Statement query(
        db_.get(),
        "SELECT verse,last_verse,text,verse_suffix FROM verse JOIN source ON source.id=verse.source_id JOIN "
        "book ON book.id=verse.book_id WHERE source.code=? AND book.code=? AND chapter=? AND verse<=? AND "
        "last_verse>=? AND verse_suffix=? ORDER BY verse DESC LIMIT 1");
    query.text(1, source);
    query.text(2, book);
    query.number(3, ref.chapter);
    query.number(4, ref.verse);
    query.number(5, ref.verse);
    query.text(6, ref.suffix);
    if (!query.row())
        return std::unexpected("Text saknas i denna utgåva");
    return Verse{{ref.chapter, query.number(0), query.text(3)},
                 query.text(2),
                 query.number(0) == query.number(1)
                     ? std::nullopt
                     : std::optional<VerseRef>{{ref.chapter, query.number(1)}}};
}
std::vector<VerseRef> CorpusDb::coordinates(const std::string& source, const std::string& book) const {
    Statement query(
        db_.get(),
        "SELECT chapter,verse,verse_suffix FROM verse JOIN source ON source.id=verse.source_id JOIN book ON "
        "book.id=verse.book_id WHERE source.code=? AND book.code=? ORDER BY chapter,verse,verse_suffix");
    query.text(1, source);
    query.text(2, book);
    std::vector<VerseRef> result;
    while (query.row())
        result.push_back({query.number(0), query.number(1), query.text(2)});
    return result;
}
std::vector<VerseRef> CorpusDb::paragraph_starts(const std::string& source, const std::string& book) const {
    Statement query(
        db_.get(),
        "SELECT chapter,verse,verse_suffix FROM paragraph JOIN source ON source.id=source_id JOIN book ON "
        "book.id=book_id WHERE source.code=? AND book.code=? ORDER BY chapter,verse,verse_suffix");
    query.text(1, source);
    query.text(2, book);
    std::vector<VerseRef> result;
    while (query.row())
        result.push_back({query.number(0), query.number(1), query.text(2)});
    return result;
}
std::optional<Alignment> CorpusDb::alignment(const std::string& from, const std::string& to,
                                             const Passage& passage) const {
    Statement query(
        db_.get(),
        "SELECT "
        "kind,from_first_chapter,from_first_verse,from_first_suffix,from_last_chapter,from_last_verse,from_"
        "last_suffix,"
        "coalesce(target.code,''),coalesce(to_first_chapter,0),coalesce(to_first_verse,0),coalesce(to_first_"
        "suffix,''),coalesce(to_last_chapter,0),coalesce(to_last_verse,0),coalesce(to_last_suffix,'') "
        "FROM alignment JOIN source f ON f.id=from_source JOIN source t ON t.id=to_source JOIN book ON "
        "book.id=book_id LEFT JOIN book target ON target.id=to_book_id "
        "WHERE f.code=? AND t.code=? AND book.code=? AND "
        "(from_first_chapter,from_first_verse,from_first_suffix)<=(?,?,?) AND "
        "(from_last_chapter,from_last_verse,from_last_suffix)>=(?,?,?) "
        "ORDER BY from_first_chapter DESC,from_first_verse DESC,from_first_suffix DESC LIMIT 1");
    query.text(1, from);
    query.text(2, to);
    query.text(3, passage.book);
    query.number(4, passage.first.chapter);
    query.number(5, passage.first.verse);
    query.text(6, passage.first.suffix);
    query.number(7, passage.last.chapter);
    query.number(8, passage.last.verse);
    query.text(9, passage.last.suffix);
    if (!query.row())
        return {};
    return Alignment{static_cast<AlignmentKind>(query.number(0)),
                     {passage.book,
                      {query.number(1), query.number(2), query.text(3)},
                      {query.number(4), query.number(5), query.text(6)}},
                     {query.text(7),
                      {query.number(8), query.number(9), query.text(10)},
                      {query.number(11), query.number(12), query.text(13)}}};
}
bool CorpusDb::same_numbering(const std::string& from, const std::string& to, const std::string& book) const {
    auto list = sources();
    const auto f = std::find_if(list.begin(), list.end(), [&](auto& s) {
        return s.code == from;
    });
    const auto t = std::find_if(list.begin(), list.end(), [&](auto& s) {
        return s.code == to;
    });
    return f != list.end() && t != list.end() &&
           (f->versification == t->versification || new_testament_book(book));
}
std::vector<std::pair<std::string, VerseRef>> CorpusDb::counterparts(const std::string& from,
                                                                     const std::string& to,
                                                                     const std::string& book,
                                                                     VerseRef ref) const {
    const auto map = alignment(from, to, {book, ref, ref});
    if (!map) {
        // Same-coordinate lookup is only safe inside matching versification systems.
        if (!same_numbering(from, to, book))
            return {};
        return {{book, ref}};
    }
    if (map->to.book.empty())
        return {};
    std::vector<std::pair<std::string, VerseRef>> result;
    if (map->from.first == map->from.last || map->kind == AlignmentKind::Merged) {
        for (auto target : coordinates(to, map->to.book))
            if (map->to.contains(target))
                result.emplace_back(map->to.book, target);
        return result;
    }
    // A range with one numbering offset.
    return {{map->to.book,
             {ref.chapter + map->to.first.chapter - map->from.first.chapter,
              ref.verse + map->to.first.verse - map->from.first.verse}}};
}
std::expected<Verse, std::string> CorpusDb::parallel_verse(const std::string& from, const std::string& to,
                                                           const std::string& book, VerseRef ref) const {
    if (from == to)
        return verse(to, book, ref);
    const auto map = alignment(from, to, {book, ref, ref});
    if (map && map->to.book.empty())
        return std::unexpected(to == "grc-lxx" ? "Saknas i Septuaginta" : "Saknas i denna utgåva");
    if (map && map->kind == AlignmentKind::Merged && ref != map->from.first)
        return std::unexpected(merged_verse);
    if (!map && !same_numbering(from, to, book))
        return std::unexpected("Ingen belagd textmappning");
    const auto targets = counterparts(from, to, book, ref);
    if (targets.empty())
        return std::unexpected("Text saknas i denna utgåva");
    Verse result{targets.front().second, "",
                 targets.size() > 1 ? std::optional<VerseRef>{targets.back().second} : std::nullopt};
    for (const auto& [target_book, target] : targets) {
        auto part = verse(to, target_book, target);
        if (!part)
            return std::unexpected(part.error());
        if (!result.text.empty())
            result.text += ' ';
        result.text += part->text;
        if (targets.size() == 1)
            result.last = part->last;
    }
    return result;
}
std::vector<Passage> CorpusDb::map_passage(const std::string& from, const std::string& to,
                                           const Passage& passage) const {
    if (from == to)
        return {passage};
    std::vector<std::pair<std::string, VerseRef>> found;
    for (auto ref : coordinates(from, passage.book))
        if (passage.contains(ref))
            for (const auto& target : counterparts(from, to, passage.book, ref))
                if (std::find(found.begin(), found.end(), target) == found.end())
                    found.push_back(target);
    // Verses that the target only reorders within one run, as Philippians 1:16-17 in
    // the 1917 Bible, read as one range in the target's own order.
    if (!found.empty() && std::all_of(found.begin(), found.end(), [&](const auto& t) {
            return t.first == found.front().first;
        })) {
        const auto refs = coordinates(to, found.front().first);
        const auto index = [&](VerseRef ref) {
            return std::find(refs.begin(), refs.end(), ref) - refs.begin();
        };
        std::vector<std::ptrdiff_t> at;
        for (const auto& target : found)
            at.push_back(index(target.second));
        if (!std::is_sorted(at.begin(), at.end()) && std::ranges::none_of(at, [&](auto i) {
                return i == std::ptrdiff_t(refs.size());
            })) {
            const auto [low, high] = std::minmax_element(at.begin(), at.end());
            bool run = true;
            for (auto i = *low; i <= *high && run; ++i)
                run = std::find(at.begin(), at.end(), i) != at.end() ||
                      counterparts(to, from, found.front().first, refs[std::size_t(i)]).empty();
            if (run)
                std::ranges::sort(found, {}, [&](const auto& target) {
                    return index(target.second);
                });
        }
    }
    // Join targets that are adjacent in the target edition, including verses between
    // them that only the target tradition has, such as lettered Septuagint additions.
    std::vector<Passage> result;
    std::map<std::string, std::vector<VerseRef>> editions;
    for (const auto& [book, ref] : found) {
        const auto& refs = editions.try_emplace(book, coordinates(to, book)).first->second;
        const auto at = std::find(refs.begin(), refs.end(), ref);
        if (at == refs.end())
            continue;
        if (!result.empty() && result.back().book == book) {
            const auto previous = std::find(refs.begin(), at, result.back().last);
            if (previous != at && std::all_of(previous + 1, at, [&](VerseRef between) {
                    return counterparts(to, from, book, between).empty();
                })) {
                result.back().last = ref;
                continue;
            }
        }
        result.push_back({book, ref, ref});
    }
    return result;
}
Reading CorpusDb::localize(Reading reading) const {
    if (!reading.source_override.empty())
        return reading;
    std::vector<Passage> parts;
    for (const auto& segment : reading.segments()) {
        const auto mapped =
            map_passage(reading.reference, frame_source(reading.base_language, segment.book), segment);
        // An edition without the book keeps the reference coordinates; the reader falls back to another
        // edition.
        const auto chosen = mapped.empty() ? std::vector<Passage>{segment} : mapped;
        parts.insert(parts.end(), chosen.begin(), chosen.end());
    }
    reading.passage = parts.front();
    reading.additional.assign(parts.begin() + 1, parts.end());
    return reading;
}
std::vector<Pronunciation> CorpusDb::pronunciations(const std::string& language) const {
    Statement query(db_.get(), "SELECT language,source,spoken,phonemes,priority FROM pronunciation WHERE "
                               "language=? ORDER BY priority DESC,length(source) DESC");
    query.text(1, language);
    std::vector<Pronunciation> result;
    while (query.row())
        result.push_back({query.text(0), query.text(1), query.text(2), query.text(3), query.number(4)});
    return result;
}
std::string CorpusDb::book_name(const std::string& book, const std::string& language) const {
    const char* sql = language == "el"   ? "SELECT name_el FROM book WHERE code=?"
                      : language == "en" ? "SELECT name_en FROM book WHERE code=?"
                                         : "SELECT name_sv FROM book WHERE code=?";
    Statement query(db_.get(), sql);
    query.text(1, book);
    return query.row() ? query.text(0) : book;
}
std::vector<Book> CorpusDb::books(const std::string& language) const {
    Statement q(db_.get(), "SELECT code,CASE WHEN ?='en' THEN name_en WHEN ?='el' AND name_el<>'' THEN "
                           "name_el ELSE name_sv END,canonical_order FROM book WHERE EXISTS(SELECT 1 FROM "
                           "verse WHERE book_id=book.id) ORDER BY canonical_order");
    q.text(1, language);
    q.text(2, language);
    std::vector<Book> result;
    while (q.row())
        result.push_back({q.text(0), q.text(1), q.number(2)});
    return result;
}
std::vector<ReadingRule> CorpusDb::reading_rules() const {
    Statement q(
        db_.get(),
        "SELECT reading_rule.id,pdist,month,day,ordering,service,description,tradition,label,source.code "
        "FROM reading_rule JOIN source ON source.id=reference ORDER BY ordering,reading_rule.id");
    std::vector<ReadingRule> result;
    while (q.row()) {
        const auto service = q.text(5);
        const auto kind = service == "Epistle"   ? ReadingKind::Epistle
                          : service == "Gospel"  ? ReadingKind::Gospel
                          : service == "Vespers" ? ReadingKind::Vespers
                                                 : ReadingKind::OldTestament;
        ReadingRule rule{q.number(0), q.number(1), q.number(2),
                         q.number(3), q.number(4), service,
                         q.text(6),   q.text(7),   {kind, {}, q.text(8), {}}};
        rule.reading.reference = q.text(9);
        Statement parts(db_.get(), "SELECT book,first_chapter,first_verse,last_chapter,last_verse FROM "
                                   "reading_segment WHERE rule_id=? ORDER BY ordering");
        parts.number(1, rule.id);
        bool first = true;
        while (parts.row()) {
            Passage p{parts.text(0), {parts.number(1), parts.number(2)}, {parts.number(3), parts.number(4)}};
            if (first) {
                rule.reading.passage = p;
                first = false;
            } else
                rule.reading.additional.push_back(p);
        }
        result.push_back(std::move(rule));
    }
    return result;
}
std::vector<FeastRule> CorpusDb::feast_rules() const {
    Statement q(
        db_.get(),
        "SELECT pdist,month,day,coalesce(rank,-100),title,feast,tradition FROM feast_rule ORDER BY id");
    std::vector<FeastRule> result;
    while (q.row())
        result.push_back(
            {q.number(0), q.number(1), q.number(2), q.number(3), q.text(4), q.text(5), q.text(6)});
    return result;
}
std::vector<OrdoRule> CorpusDb::ordo_rules() const {
    Statement q(db_.get(), "SELECT year,month,day,pdist,service FROM ordo_rule");
    std::vector<OrdoRule> result;
    while (q.row())
        result.push_back({q.number(0), q.number(1), q.number(2), q.number(3), q.text(4)});
    return result;
}
} // namespace ortho
