#include "storage/database.hpp"
#include "storage/sqlite.hpp"
// The corpus's word-study queries.
namespace ortho {
using storage::Statement;
std::vector<DalinEntry> CorpusDb::swedish(const std::string& form) const {
    Statement query(db_.get(), "SELECT headword,gram,definition FROM sv_word JOIN dalin ON "
                               "dalin.id=sv_word.dalin WHERE form=? ORDER BY dalin.id");
    query.text(1, form);
    std::vector<DalinEntry> result;
    while (query.row())
        result.push_back({query.text(0), query.text(1), query.text(2)});
    return result;
}
std::vector<BiblicalEntry> CorpusDb::biblical(const std::string& form) const {
    Statement query(db_.get(),
                    "SELECT biblical.id,headword,definition,url FROM sv_biblical JOIN biblical ON "
                    "biblical.id=sv_biblical.article WHERE form=? ORDER BY sv_biblical.rank,biblical.id");
    query.text(1, form);
    std::vector<BiblicalEntry> result;
    while (query.row())
        result.push_back({query.text(0), query.text(1), query.text(2), query.text(3)});
    return result;
}
namespace {
std::vector<ModernWordEntry> modern_words(sqlite3* db, const char* sql, const std::string& form) {
    Statement query(db, sql);
    query.text(1, form);
    std::vector<ModernWordEntry> result;
    while (query.row())
        result.push_back({query.text(0), query.text(1), query.text(2), query.text(3), query.text(4)});
    return result;
}
} // namespace
std::vector<ModernWordEntry> CorpusDb::saldo(const std::string& form) const {
    return modern_words(db_.get(),
                        "SELECT saldo.id,headword,gram,associations,match FROM sv_saldo JOIN saldo "
                        "ON saldo.id=sv_saldo.entry WHERE form=? ORDER BY saldo.id",
                        form);
}
std::vector<ModernWordEntry> CorpusDb::academic(const std::string& form) const {
    return modern_words(db_.get(),
                        "SELECT academic.id,headword,gram,'',match FROM sv_academic JOIN academic "
                        "ON academic.id=sv_academic.entry WHERE form=? ORDER BY academic.id",
                        form);
}
} // namespace ortho
