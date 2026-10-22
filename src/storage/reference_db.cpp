#include "storage/database.hpp"
#include "storage/sqlite.hpp"
#include <algorithm>
#include <stdexcept>

namespace ortho {
std::string CorpusDb::reference_name(const std::string& book) const {
    static const std::map<std::string, std::string> names{{"1Sam", "1 Kingdoms"},
                                                          {"2Sam", "2 Kingdoms"},
                                                          {"1Kgs", "3 Kingdoms"},
                                                          {"2Kgs", "4 Kingdoms"},
                                                          {"1Esd", "1 Ezra"},
                                                          {"Ezra", "2 Ezra"},
                                                          {"Song", "Song of Songs"},
                                                          {"Wis", "Wisdom of Solomon"},
                                                          {"Sir", "Wisdom of Sirach"},
                                                          {"Sus", "Daniel: Susanna"},
                                                          {"Bel", "Daniel: Bel and the Dragon"}};
    const auto found = names.find(book);
    return found == names.end() ? book_name(book, "en") : found->second;
}
void CorpusDb::load_reference(const std::filesystem::path& path) {
    reference_ = storage::open(path, SQLITE_OPEN_READONLY);
    storage::exec(reference_.get(), "PRAGMA query_only=ON");
    if (storage::pragma_number(reference_.get(), "PRAGMA user_version") != 1 ||
        storage::pragma_number(reference_.get(), "PRAGMA application_id") != 0x4f525452)
        throw std::runtime_error("Unsupported OSB reference metadata");
    canon_.clear();
    storage::Statement query(reference_.get(),
                             "SELECT book,name,abbreviation,last_chapter FROM canon ORDER BY position");
    while (query.row())
        canon_.push_back({query.text(0), query.text(0), query.text(1), query.text(2), 1, query.number(3),
                          new_testament_book(query.text(0))});
    versification_.emplace(osb_reference, "OSB-2008-v1");
}

const CorpusDb::ReferenceLinks& CorpusDb::reference_links(const std::string& from, const std::string& to,
                                                          const std::string& book) const {
    std::lock_guard lock(cache_mutex_);
    auto [entry, added] = reference_links_.try_emplace({from, to, book});
    if (!added)
        return entry->second;
    const bool forward = from == osb_reference;
    const auto sql =
        forward ? "SELECT c.chapter,c.verse,c.suffix,v.book,v.chapter,v.verse,v.suffix "
                  "FROM coordinate c JOIN link l ON l.osb=c.id JOIN source_verse v ON v.id=l.target "
                  "JOIN source s ON s.id=v.source WHERE c.book=? AND s.code=? ORDER BY c.ordinal,v.ordinal"
                : "SELECT v.chapter,v.verse,v.suffix,c.book,c.chapter,c.verse,c.suffix "
                  "FROM source_verse v JOIN source s ON s.id=v.source JOIN link l ON l.target=v.id "
                  "JOIN coordinate c ON c.id=l.osb WHERE v.book=? AND s.code=? ORDER BY v.ordinal,c.ordinal";
    storage::Statement query(reference_.get(), sql);
    query.text(1, book);
    query.text(2, forward ? to : from);
    while (query.row())
        entry->second[{query.number(0), query.number(1), query.text(2)}].push_back(
            {query.text(3), {query.number(4), query.number(5), query.text(6)}});
    return entry->second;
}
} // namespace ortho
