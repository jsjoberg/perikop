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
    if (pragma_number(db_.get(), "PRAGMA user_version") != 4 ||
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
} // namespace ortho
