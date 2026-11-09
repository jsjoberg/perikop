#include "storage/database.hpp"
#include "storage/sqlite.hpp"
// The corpus's word-study query.
namespace ortho {
using storage::Statement;
std::vector<WordSense> CorpusDb::senses(const std::string& form) const {
    const char* sql = "SELECT kind,headword,gloss,url,phonemes,usual FROM word JOIN sense ON sense.id=word.sense "
                      "WHERE form=? ORDER BY rank";
    std::vector<WordSense> result;
    // The supplement holds only the forms the bundled corpus lacks.
    for (auto* db : {db_.get(), study_.get()}) {
        if (!db || !result.empty())
            continue;
        Statement query(db, sql);
        query.text(1, form);
        while (query.row())
            result.push_back({query.text(0), query.text(1), query.text(2), query.text(3), query.text(4),
                              query.number(5) != 0});
    }
    return result;
}
} // namespace ortho
