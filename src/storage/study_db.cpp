#include "storage/database.hpp"
#include "storage/sqlite.hpp"
// The corpus's word-study queries.
namespace ortho {
using storage::Statement;
sqlite3* CorpusDb::study_database(const char* table, const std::string& form) const {
    if (!study_)
        return db_.get();
    const auto sql = std::string("SELECT 1 FROM ") + table + " WHERE form=? LIMIT 1";
    Statement known(db_.get(), sql.c_str());
    known.text(1, form);
    return known.row() ? db_.get() : study_.get();
}
std::vector<DalinEntry> CorpusDb::swedish(const std::string& form) const {
    Statement query(study_database("sv_word", form),
                    "SELECT headword,gram,definition FROM sv_word JOIN dalin ON "
                    "dalin.id=sv_word.dalin WHERE form=? ORDER BY dalin.id");
    query.text(1, form);
    std::vector<DalinEntry> result;
    while (query.row())
        result.push_back({query.text(0), query.text(1), query.text(2)});
    return result;
}
std::vector<BiblicalEntry> CorpusDb::biblical(const std::string& form) const {
    Statement query(study_database("sv_biblical", form),
                    "SELECT biblical.id,headword,definition,url FROM sv_biblical JOIN biblical ON "
                    "biblical.id=sv_biblical.article WHERE form=? ORDER BY sv_biblical.rank,biblical.id");
    query.text(1, form);
    std::vector<BiblicalEntry> result;
    while (query.row())
        result.push_back({query.text(0), query.text(1), query.text(2), query.text(3)});
    return result;
}
std::vector<ModernWordEntry> CorpusDb::saldo(const std::string& form) const {
    Statement query(study_database("sv_saldo", form),
                    "SELECT saldo.id,headword,gram,associations,match FROM sv_saldo JOIN saldo "
                    "ON saldo.id=sv_saldo.entry WHERE form=? ORDER BY saldo.id");
    query.text(1, form);
    std::vector<ModernWordEntry> result;
    while (query.row())
        result.push_back({query.text(0), query.text(1), query.text(2), query.text(3), query.text(4)});
    return result;
}
} // namespace ortho
