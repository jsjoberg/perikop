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
    if (pragma_number(db_.get(), "PRAGMA user_version") != 10 ||
        pragma_number(db_.get(), "PRAGMA application_id") != 0x4f525443)
        throw std::runtime_error("Unsupported corpus schema");
    for (const auto& source : sources())
        versification_.emplace(source.code, source.versification);
    Statement books(db_.get(), "SELECT code,abbreviation_sv,new_testament,deuterocanonical,stanzas,"
                               "name_sv,name_en,name_el FROM book");
    while (books.row())
        books_.emplace(books.text(0), BookInfo{books.text(1),
                                               books.number(2) == 1,
                                               books.number(3) == 1,
                                               books.number(4) == 1,
                                               {books.text(5), books.text(6), books.text(7)}});
    Statement canon(db_.get(), "SELECT book,frame_book,coalesce(canon.name_sv,book.name_sv),"
                               "coalesce(canon.abbreviation_sv,book.abbreviation_sv),first_chapter,"
                               "coalesce(last_chapter,999) FROM canon JOIN book ON book.code=canon.book "
                               "ORDER BY position");
    while (canon.row())
        canon_.push_back({canon.text(0), canon.text(1), canon.text(2), canon.text(3), canon.number(4),
                          canon.number(5), new_testament_book(canon.text(0))});
}
bool CorpusDb::new_testament_book(const std::string& book) const {
    const auto it = books_.find(book);
    return it != books_.end() && it->second.new_testament;
}
bool CorpusDb::deuterocanonical_book(const std::string& book) const {
    const auto it = books_.find(book);
    return it != books_.end() && it->second.deuterocanonical;
}
bool CorpusDb::stanza_book(const std::string& book) const {
    const auto it = books_.find(book);
    return it != books_.end() && it->second.stanzas;
}
std::string CorpusDb::book_abbreviation(const std::string& book) const {
    const auto it = books_.find(book);
    return it == books_.end() ? book : it->second.abbreviation;
}
const CanonBook* CorpusDb::canon_book(const std::string& frame_book, int chapter) const {
    for (const auto& book : canon_)
        if (book.frame_book == frame_book && book.first_chapter <= chapter && chapter <= book.last_chapter)
            return &book;
    return nullptr;
}
std::string CorpusDb::title(const std::string& book) const {
    const auto it = std::ranges::find(canon_, book, &CanonBook::code);
    return it == canon_.end() ? book_name(book) : it->name;
}
std::string CorpusDb::short_title(const std::string& book) const {
    const auto it = std::ranges::find(canon_, book, &CanonBook::code);
    return it == canon_.end() ? book_abbreviation(book) : it->abbreviation;
}
std::string CorpusDb::source_for_language(const std::string& language, const std::string& book) const {
    if (language == "sv")
        return "sv1917";
    // LXX2012 translates the Septuagint; WEB supplies the New Testament, as on Orthocal.
    if (language == "en")
        return new_testament_book(book) ? "en-web" : "en-lxx2012";
    if (language == "el")
        return new_testament_book(book) ? "grc-patriarchal" : "grc-lxx";
    return {};
}
std::string CorpusDb::frame_source(const std::string& language, const std::string& book) const {
    return new_testament_book(book) ? source_for_language(language, book) : "grc-lxx";
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
const CorpusDb::BookText& CorpusDb::book_text(const std::string& source, const std::string& book) const {
    std::lock_guard lock(cache_mutex_);
    auto [entry, added] = texts_.try_emplace({source, book});
    if (!added)
        return entry->second;
    Statement query(db_.get(),
                    "SELECT chapter,verse,verse_suffix,last_verse,text FROM verse JOIN source ON "
                    "source.id=verse.source_id JOIN book ON book.id=verse.book_id WHERE source.code=? "
                    "AND book.code=? ORDER BY chapter,verse,verse_suffix");
    query.text(1, source);
    query.text(2, book);
    auto& text = entry->second;
    while (query.row()) {
        text.coordinates.push_back({query.number(0), query.number(1), query.text(2)});
        text.last_verses.push_back(query.number(3));
        text.texts.push_back(query.text(4));
    }
    return text;
}
std::expected<Verse, std::string> CorpusDb::verse(const std::string& source, const std::string& book,
                                                  VerseRef ref) const {
    // The verse row that covers the number: the last one in the chapter that starts at or before it.
    const auto& text = book_text(source, book);
    const auto& refs = text.coordinates;
    auto at = std::upper_bound(refs.begin(), refs.end(), VerseRef{ref.chapter, ref.verse, "\x7f"});
    while (at != refs.begin() && (at - 1)->chapter == ref.chapter) {
        --at;
        const auto i = std::size_t(at - refs.begin());
        if (at->suffix == ref.suffix && text.last_verses[i] >= ref.verse)
            return Verse{*at, text.texts[i],
                         text.last_verses[i] == at->verse
                             ? std::nullopt
                             : std::optional<VerseRef>{{ref.chapter, text.last_verses[i]}}};
    }
    return std::unexpected("Text saknas i denna utgåva");
}
const std::vector<VerseRef>& CorpusDb::coordinates(const std::string& source, const std::string& book) const {
    return book_text(source, book).coordinates;
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
const std::vector<Alignment>& CorpusDb::book_alignment(const std::string& from, const std::string& to,
                                                       const std::string& book) const {
    std::lock_guard lock(cache_mutex_);
    auto [entry, added] = alignments_.try_emplace({from, to, book});
    if (!added)
        return entry->second;
    Statement query(
        db_.get(),
        "SELECT "
        "kind,from_first_chapter,from_first_verse,from_first_suffix,from_last_chapter,from_last_verse,from_"
        "last_suffix,"
        "coalesce(target.code,''),coalesce(to_first_chapter,0),coalesce(to_first_verse,0),coalesce(to_first_"
        "suffix,''),coalesce(to_last_chapter,0),coalesce(to_last_verse,0),coalesce(to_last_suffix,'') "
        "FROM alignment JOIN source f ON f.id=from_source JOIN source t ON t.id=to_source JOIN book ON "
        "book.id=book_id LEFT JOIN book target ON target.id=to_book_id "
        "WHERE f.code=? AND t.code=? AND book.code=? "
        "ORDER BY from_first_chapter,from_first_verse,from_first_suffix");
    query.text(1, from);
    query.text(2, to);
    query.text(3, book);
    while (query.row())
        entry->second.push_back({static_cast<AlignmentKind>(query.number(0)),
                                 {book,
                                  {query.number(1), query.number(2), query.text(3)},
                                  {query.number(4), query.number(5), query.text(6)}},
                                 {query.text(7),
                                  {query.number(8), query.number(9), query.text(10)},
                                  {query.number(11), query.number(12), query.text(13)}}});
    return entry->second;
}
std::optional<Alignment> CorpusDb::alignment(const std::string& from, const std::string& to,
                                             const Passage& passage) const {
    // The row with the latest start at or before the passage that also reaches its end.
    const auto& rows = book_alignment(from, to, passage.book);
    auto at = std::upper_bound(rows.begin(), rows.end(), passage.first,
                               [](const VerseRef& first, const Alignment& row) {
                                   return first < row.from.first;
                               });
    while (at != rows.begin()) {
        --at;
        if (at->from.last >= passage.last)
            return *at;
    }
    return {};
}
bool CorpusDb::same_numbering(const std::string& from, const std::string& to, const std::string& book) const {
    const auto f = versification_.find(from), t = versification_.find(to);
    return f != versification_.end() && t != versification_.end() &&
           (f->second == t->second || new_testament_book(book));
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
        return std::unexpected(coordinates(to, book).empty() ? "Saknas i denna utgåva"
                                                             : "Ingen belagd textmappning");
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
        auto edition = editions.find(book);
        if (edition == editions.end())
            edition = editions.emplace(book, coordinates(to, book)).first;
        const auto& refs = edition->second;
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
        const auto mapped = map_passage(
            reading.reference.empty() ? frame_source(reading.base_language, segment.book) : reading.reference,
            frame_source(reading.base_language, segment.book), segment);
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
    const auto found = books_.find(book);
    return found == books_.end() ? book
                                 : found->second.names[language == "el"   ? 2
                                                       : language == "en" ? 1
                                                                          : 0];
}
std::vector<ReadingRule> CorpusDb::reading_rules() const {
    Statement q(db_.get(),
                "SELECT "
                "reading_rule.id,pdist,month,day,ordering,service,description,tradition,source.code,label,"
                "reading_rule.year,reading_rule.source_url "
                "FROM reading_rule JOIN source ON source.id=reference ORDER BY ordering,reading_rule.id");
    std::vector<ReadingRule> result;
    while (q.row()) {
        const auto service = q.text(5);
        const auto kind =
            service == "Epistle"             ? ReadingKind::Epistle
            : service == "Gospel"            ? ReadingKind::Gospel
            : service.starts_with("Vespers") ? ReadingKind::Vespers
            : service.find("Matins") != std::string::npos || service.find("Passion") != std::string::npos
                ? ReadingKind::Matins
            : service.find("Hour") != std::string::npos ? ReadingKind::Hours
            : service == "Great Blessing of Waters" || service == "Cross Procession"
                ? ReadingKind::OtherService
                : ReadingKind::OldTestament;
        ReadingRule rule{q.number(0), q.number(1), q.number(2), q.number(3), q.number(4),
                         service,     q.text(6),   q.text(7),   {kind, {}}};
        rule.year = q.number(10);
        rule.reading.assignment_source_url = q.text(11);
        rule.reading.reference = q.text(8);
        rule.reading.service = service;
        rule.reading.occasion = q.text(6);
        rule.reading.citation = q.text(9);
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
    Statement q(db_.get(), "SELECT pdist,month,day,coalesce(rank,-100),tradition,cycle_title "
                           "FROM feast_rule ORDER BY id");
    std::vector<FeastRule> result;
    while (q.row())
        result.push_back({q.number(0), q.number(1), q.number(2), q.number(3), q.text(4), q.text(5)});
    return result;
}
std::vector<OrdoRule> CorpusDb::ordo_rules() const {
    Statement q(db_.get(), "SELECT year,month,day,pdist,service,jurisdiction FROM ordo_rule");
    std::vector<OrdoRule> result;
    while (q.row())
        result.push_back({q.number(0), q.number(1), q.number(2), q.number(3), q.text(4), q.text(5)});
    return result;
}

} // namespace ortho
