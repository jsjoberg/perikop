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
    if (pragma_number(db_.get(), "PRAGMA user_version") != 5 ||
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
std::vector<ModernWordEntry> StudyDb::saldo(const std::string& form) const {
    return modern_words(db_.get(),
                        "SELECT saldo.id,headword,gram,associations,match FROM sv_saldo JOIN saldo "
                        "ON saldo.id=sv_saldo.entry WHERE form=? ORDER BY saldo.id",
                        form);
}
std::vector<ModernWordEntry> StudyDb::academic(const std::string& form) const {
    return modern_words(db_.get(),
                        "SELECT academic.id,headword,gram,'',match FROM sv_academic JOIN academic "
                        "ON academic.id=sv_academic.entry WHERE form=? ORDER BY academic.id",
                        form);
}
} // namespace ortho
