#include "speech/speech.hpp"
#include "storage/content_import.hpp"
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
    const auto supplement = path.parent_path().parent_path() / "content/swedish-study.db";
    if (std::filesystem::exists(supplement)) {
        study_ = open(supplement, SQLITE_OPEN_READONLY);
        exec(study_.get(), "PRAGMA query_only=ON");
        if (pragma_number(study_.get(), "PRAGMA user_version") != 1 ||
            pragma_number(study_.get(), "PRAGMA application_id") != 1330795603)
            throw std::runtime_error("Unsupported Swedish dictionary supplement");
    }
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
    load_reference(path.parent_path() / "osb-reference.db");
}
bool CorpusDb::new_testament_book(const std::string& book) const {
    const auto it = books_.find(book);
    return it != books_.end() && it->second.new_testament;
}
bool CorpusDb::deuterocanonical_book(const std::string& book) const {
    const auto it = books_.find(book);
    return it != books_.end() && it->second.deuterocanonical;
}
bool CorpusDb::stanza_book(const std::string& book, int chapter) const {
    if (book == "2Chr" && chapter == 37)
        return true;
    const auto it = books_.find(book);
    return it != books_.end() && it->second.stanzas;
}
std::string CorpusDb::book_abbreviation(const std::string& book) const {
    const auto it = books_.find(book);
    return it == books_.end() ? book : it->second.abbreviation;
}
const CanonBook* CorpusDb::canon_book(const std::string& book, int chapter) const {
    const auto it = std::ranges::find(canon_, book, &CanonBook::code);
    return it != canon_.end() && 1 <= chapter && chapter <= it->last_chapter ? &*it : nullptr;
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
bool CorpusDb::read_only() const {
    return sqlite3_db_readonly(db_.get(), "main") == 1;
}
void CorpusDb::load_imported(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path))
        return;
    auto candidate = open(path, SQLITE_OPEN_READONLY);
    exec(candidate.get(), "PRAGMA query_only=ON");
    Statement schema(candidate.get(),
                     "SELECT 1 FROM sqlite_master WHERE type='table' AND name='content_metadata'");
    if (!schema.row())
        return;
    Statement valid(candidate.get(), "SELECT value FROM content_metadata WHERE key='srb2026'");
    if (!valid.row())
        return; // An interrupted first import has no committed installation.
    if (valid.text(0).size() != 64)
        throw std::runtime_error("Ogiltigt lokalt bibelinnehåll. Importera PDF-filerna igen.");
    Statement count(candidate.get(), "SELECT count(*),count(DISTINCT book) FROM imported_verse");
    if (!count.row() || count.number(0) != 31171 || count.number(1) != 66)
        throw std::runtime_error("Det lokala bibelinnehållet är ofullständigt. Importera PDF-filerna igen.");
    std::lock_guard lock(cache_mutex_);
    imported_ = std::move(candidate);
    versification_[srb_source] = "SRB2026";
    clear_imported_cache();
}
void CorpusDb::remove_imported() {
    std::lock_guard lock(cache_mutex_);
    if (!imported_)
        return;
    const auto* filename = sqlite3_db_filename(imported_.get(), "main");
    const std::filesystem::path path(reinterpret_cast<const char8_t*>(filename));
    // Windows requires closing the database before deleting its file.
    imported_.reset();
    std::error_code error;
    std::filesystem::remove(path, error);
    if (error) {
        imported_ = open(path, SQLITE_OPEN_READONLY);
        exec(imported_.get(), "PRAGMA query_only=ON");
        throw std::filesystem::filesystem_error("Det importerade bibelinnehållet kunde inte tas bort", path,
                                                error);
    }
    versification_.erase(srb_source);
    clear_imported_cache();
}
void CorpusDb::clear_imported_cache() {
    TextCache retired;
    for (auto it = texts_.begin(); it != texts_.end();) {
        if (it->first.first == srb_source) {
            auto book = texts_.extract(it++);
            book.mapped().texts.clear();
            retired.insert(std::move(book));
        } else
            ++it;
    }
    if (!retired.empty())
        retired_texts_.push_back(std::move(retired));
    std::erase_if(alignments_, [](const auto& entry) {
        return std::get<0>(entry.first) == srb_source || std::get<1>(entry.first) == srb_source;
    });
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
    if (imported_)
        result.push_back({100, srb_source, "sv", "Reformationsbibeln 2026", "SRB2026"});
    return result;
}
std::vector<std::string> CorpusDb::source_books(const std::string& source) const {
    std::lock_guard lock(cache_mutex_);
    std::vector<std::string> result;
    if (source == srb_source) {
        if (imported_) {
            Statement query(imported_.get(), "SELECT DISTINCT book FROM imported_verse ORDER BY book");
            while (query.row())
                result.push_back(query.text(0));
        }
        return result;
    }
    Statement query(db_.get(), "SELECT book.code FROM book JOIN source ON source.code=? WHERE EXISTS "
                               "(SELECT 1 FROM verse WHERE source_id=source.id AND book_id=book.id) "
                               "ORDER BY book.code");
    query.text(1, source);
    while (query.row())
        result.push_back(query.text(0));
    return result;
}
const CorpusDb::BookText& CorpusDb::book_text(const std::string& source, const std::string& book) const {
    std::lock_guard lock(cache_mutex_);
    auto [entry, added] = texts_.try_emplace({source, book});
    if (!added)
        return entry->second;
    if (source == osb_reference) {
        Statement query(reference_.get(),
                        "SELECT chapter,verse,suffix FROM coordinate WHERE book=? ORDER BY ordinal");
        query.text(1, book);
        auto& text = entry->second;
        while (query.row()) {
            const VerseRef ref{query.number(0), query.number(1), query.text(2)};
            text.order.emplace(ref, text.coordinates.size());
            text.coordinates.push_back(ref);
        }
        return text;
    }
    if (source == srb_source) {
        if (imported_) {
            Statement schema(imported_.get(),
                             "SELECT 1 FROM sqlite_master WHERE type='table' AND name='imported_heading'");
            if (schema.row()) {
                Statement headings(imported_.get(), "SELECT chapter,verse,text FROM imported_heading WHERE "
                                                    "book=? ORDER BY chapter,verse,ordinal");
                headings.text(1, book);
                while (headings.row())
                    entry->second.headings[{headings.number(0), headings.number(1)}].push_back(
                        headings.text(2));
            }
            Statement query(
                imported_.get(),
                "SELECT chapter,verse,text FROM imported_verse WHERE book=? ORDER BY chapter,verse");
            query.text(1, book);
            while (query.row()) {
                entry->second.coordinates.push_back({query.number(0), query.number(1)});
                entry->second.last_verses.push_back(query.number(1));
                entry->second.texts.push_back(query.text(2));
            }
        }
        return entry->second;
    }
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
    if (source == osb_reference)
        return std::unexpected("OSB är referensramen, inte en installerad bibeltext");
    // The verse row that covers the number: the last one in the chapter that starts at or before it.
    const auto& text = book_text(source, book);
    const auto& refs = text.coordinates;
    auto at = std::upper_bound(refs.begin(), refs.end(), VerseRef{ref.chapter, ref.verse, "\x7f"});
    while (at != refs.begin() && (at - 1)->chapter == ref.chapter) {
        --at;
        const auto i = std::size_t(at - refs.begin());
        if (at->suffix == ref.suffix && text.last_verses[i] >= ref.verse) {
            const auto headings = text.headings.find(*at);
            return Verse{*at, text.texts[i],
                         text.last_verses[i] == at->verse
                             ? std::nullopt
                             : std::optional<VerseRef>{{ref.chapter, text.last_verses[i]}},
                         book,
                         headings == text.headings.end() ? std::vector<std::string>{} : headings->second};
        }
    }
    return std::unexpected("Text saknas i denna utgåva");
}
const std::vector<VerseRef>& CorpusDb::coordinates(const std::string& source, const std::string& book) const {
    return book_text(source, book).coordinates;
}
int CorpusDb::compare(const std::string& source, const std::string& book, VerseRef a, VerseRef b) const {
    if (source == osb_reference) {
        const auto& order = book_text(source, book).order;
        const auto ai = order.find(a), bi = order.find(b);
        if (ai != order.end() && bi != order.end())
            return ai->second < bi->second ? -1 : ai->second > bi->second ? 1 : 0;
    }
    return a < b ? -1 : a > b ? 1 : 0;
}
bool CorpusDb::contains(const std::string& source, const Passage& passage, VerseRef ref) const {
    return compare(source, passage.book, passage.first, ref) <= 0 &&
           compare(source, passage.book, ref, passage.last) <= 0;
}
std::vector<VerseRef> CorpusDb::paragraph_starts(const std::string& source, const std::string& book) const {
    if (source == osb_reference) {
        std::vector<VerseRef> result;
        for (const auto& ref : coordinates(source, book))
            if (ref.verse == 1 || stanza_book(book, ref.chapter))
                result.push_back(ref);
        return result;
    }
    if (source == srb_source) {
        std::vector<VerseRef> result;
        for (const auto& ref : coordinates(source, book))
            if (ref.verse == 1 || stanza_book(book))
                result.push_back(ref);
        return result;
    }
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
    if (from == srb_source || to == srb_source) {
        if (imported_) {
            Statement query(
                imported_.get(),
                "SELECT kind,fc,fv,fs,lc,lv,ls,tbook,tfc,tfv,tfs,tlc,tlv,tls "
                "FROM imported_alignment WHERE source=? AND target=? AND book=? ORDER BY fc,fv,fs");
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
        }
        return entry->second;
    }
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
    if (from == osb_reference || to == osb_reference) {
        if (passage.first != passage.last)
            return {};
        const auto& links = reference_links(from, to, passage.book);
        const auto found = links.find(passage.first);
        if (found == links.end() || found->second.empty())
            return Alignment{AlignmentKind::LxxOnly, passage, {"", {0, 0}, {0, 0}}};
        const auto& targets = found->second;
        if (std::ranges::any_of(targets, [&](const auto& target) {
                return target.first != targets.front().first;
            }))
            return {};
        const auto& refs = coordinates(from, passage.book);
        auto first = std::find(refs.begin(), refs.end(), passage.first), last = first;
        while (first != refs.begin() && first != refs.end()) {
            const auto previous = links.find(*(first - 1));
            if (previous == links.end() || previous->second != targets)
                break;
            --first;
        }
        while (last != refs.end() && last + 1 != refs.end()) {
            const auto next = links.find(*(last + 1));
            if (next == links.end() || next->second != targets)
                break;
            ++last;
        }
        const Passage span{passage.book, first == refs.end() ? passage.first : *first,
                           last == refs.end() ? passage.last : *last};
        const auto kind = span.first != span.last ? AlignmentKind::Merged
                          : targets.size() > 1    ? AlignmentKind::Split
                          : targets.front() == std::pair{passage.book, passage.first}
                              ? AlignmentKind::Same
                              : AlignmentKind::Renumbered;
        return Alignment{kind, span, {targets.front().first, targets.front().second, targets.back().second}};
    }
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
    if (from == osb_reference || to == osb_reference)
        return false;
    const auto f = versification_.find(from), t = versification_.find(to);
    return f != versification_.end() && t != versification_.end() &&
           (f->second == t->second || new_testament_book(book));
}
std::vector<std::pair<std::string, VerseRef>> CorpusDb::counterparts(const std::string& from,
                                                                     const std::string& to,
                                                                     const std::string& book,
                                                                     VerseRef ref) const {
    if (from == to)
        return {{book, ref}};
    if (from == osb_reference || to == osb_reference) {
        const auto& links = reference_links(from, to, book);
        const auto it = links.find(ref);
        return it == links.end() ? ReferenceTargets{} : it->second;
    }
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
            if (contains(to, map->to, target))
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
    const bool reference_mapping = from == osb_reference || to == osb_reference;
    const auto map = alignment(from, to, {book, ref, ref});
    if (map && map->to.book.empty())
        return std::unexpected(to == "grc-lxx" ? "Saknas i Septuaginta" : "Saknas i denna utgåva");
    if (map && map->kind == AlignmentKind::Merged && ref != map->from.first)
        return std::unexpected(merged_verse);
    if (!map && !reference_mapping && !same_numbering(from, to, book))
        return std::unexpected(coordinates(to, book).empty() ? "Saknas i denna utgåva"
                                                             : "Ingen belagd textmappning");
    const auto targets = counterparts(from, to, book, ref);
    if (targets.empty())
        return std::unexpected(reference_mapping ? "Ingen belagd motsvarighet i denna utgåva"
                                                 : "Text saknas i denna utgåva");
    if (reference_mapping && !map) {
        const auto& refs = coordinates(from, book);
        const auto at = std::find(refs.begin(), refs.end(), ref);
        if (at != refs.begin() && at != refs.end() && counterparts(from, to, book, *(at - 1)) == targets)
            return std::unexpected(merged_verse);
    }
    Verse result{targets.front().second, "",
                 targets.size() > 1 ? std::optional<VerseRef>{targets.back().second} : std::nullopt,
                 targets.front().first};
    for (const auto& [target_book, target] : targets) {
        auto part = verse(to, target_book, target);
        if (!part)
            return std::unexpected(part.error());
        if (!result.text.empty())
            result.text += ' ';
        result.text += part->text;
        result.headings.insert(result.headings.end(), part->headings.begin(), part->headings.end());
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
        if (contains(from, passage, ref))
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
    std::vector<Passage> parts;
    for (const auto& segment : reading.segments()) {
        const auto from =
            reading.reference.empty()
                ? (reading.source_override.empty() ? std::string(osb_reference) : reading.source_override)
                : reading.reference;
        if (from == osb_reference) {
            const auto& refs = coordinates(osb_reference, segment.book);
            if (std::ranges::find(refs, segment.first) == refs.end() ||
                std::ranges::find(refs, segment.last) == refs.end())
                throw std::runtime_error("Referensen finns inte i OSB.");
        }
        const auto mapped = map_passage(from, osb_reference, segment);
        if (mapped.empty())
            throw std::runtime_error("Ingen belagd OSB-mappning för detta avsnitt.");
        parts.insert(parts.end(), mapped.begin(), mapped.end());
    }
    if (parts.empty())
        throw std::runtime_error("Ingen belagd OSB-mappning för detta avsnitt.");
    reading.passage = parts.front();
    reading.additional.assign(parts.begin() + 1, parts.end());
    reading.reference = osb_reference;
    return reading;
}
std::vector<Pronunciation> CorpusDb::pronunciations(const std::string& language) const {
    Statement query(db_.get(), "SELECT language,source,spoken,phonemes,priority FROM pronunciation WHERE "
                               "language=? ORDER BY priority DESC,length(source) DESC");
    query.text(1, language);
    std::vector<Pronunciation> result;
    while (query.row())
        result.push_back({query.text(0), query.text(1), query.text(2), query.text(3), query.number(4)});
    if (study_) {
        Statement extra(
            study_.get(),
            "SELECT language,source,spoken,phonemes,priority FROM pronunciation WHERE language=?");
        extra.text(1, language);
        while (extra.row())
            result.push_back({extra.text(0), extra.text(1), extra.text(2), extra.text(3), extra.number(4)});
    }
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
