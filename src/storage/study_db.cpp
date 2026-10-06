#include "storage/database.hpp"
#include "storage/sqlite.hpp"
#include <stdexcept>
namespace ortho {
using storage::exec;
using storage::open;
using storage::pragma_number;
using storage::Statement;
using storage::Transaction;
StudyDb::StudyDb(const std::filesystem::path& path) : db_(open(path, SQLITE_OPEN_READONLY)) {
    exec(db_.get(), "PRAGMA query_only=ON");
    if (pragma_number(db_.get(), "PRAGMA user_version") != 3 ||
        pragma_number(db_.get(), "PRAGMA application_id") != 0x4f525354)
        throw std::runtime_error("Unsupported word-study schema");
}
std::vector<DalinEntry> StudyDb::swedish(const std::string& form) const {
    Statement query(db_.get(), "SELECT headword,gram,definition FROM sv_word JOIN dalin ON "
                               "dalin.id=sv_word.dalin WHERE form=? ORDER BY dalin.id");
    query.text(1, form);
    std::vector<DalinEntry> result;
    while (query.row())
        result.push_back({query.text(0), query.text(1), query.text(2)});
    return result;
}
std::vector<BiblicalEntry> StudyDb::biblical(const std::string& form) const {
    Statement query(db_.get(),
                    "SELECT biblical.id,headword,definition,url FROM sv_biblical JOIN biblical ON "
                    "biblical.id=sv_biblical.article WHERE form=? ORDER BY sv_biblical.rank,biblical.id");
    query.text(1, form);
    std::vector<BiblicalEntry> result;
    while (query.row())
        result.push_back({query.text(0), query.text(1), query.text(2), query.text(3)});
    return result;
}
std::optional<std::pair<VerseRef, int>> StudyDb::greek_link(const std::string& book, VerseRef ref,
                                                            const std::string& form, int occurrence) const {
    Statement query(db_.get(), "SELECT greek_chapter,greek_verse,greek_suffix,position FROM sv_greek WHERE "
                               "book=? AND chapter=? AND verse=? AND form=? AND occurrence=?");
    query.text(1, book);
    query.number(2, ref.chapter);
    query.number(3, ref.verse);
    query.text(4, form);
    query.number(5, occurrence);
    if (!query.row())
        return std::nullopt;
    return std::pair{VerseRef{query.number(0), query.number(1), query.text(2)}, query.number(3)};
}
std::optional<StrongsEntry> StudyDb::strongs(const std::string& strong) const {
    Statement query(db_.get(),
                    "SELECT strong,lemma,transliteration,gloss,definition FROM strongs WHERE strong=?");
    query.text(1, strong);
    if (!query.row())
        return std::nullopt;
    return StrongsEntry{query.text(0), query.text(1), query.text(2), query.text(3), query.text(4)};
}
std::vector<GreekWord> StudyDb::greek_words(const std::string& book, VerseRef ref) const {
    Statement query(db_.get(), "SELECT surface,strong FROM greek_word WHERE book=? AND chapter=? AND verse=? "
                               "AND suffix=? ORDER BY position");
    query.text(1, book);
    query.number(2, ref.chapter);
    query.number(3, ref.verse);
    query.text(4, ref.suffix);
    std::vector<GreekWord> result;
    while (query.row())
        result.push_back({query.text(0), query.text(1)});
    return result;
}
} // namespace ortho
