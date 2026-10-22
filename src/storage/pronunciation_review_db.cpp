#include "speech/speech.hpp"
#include "storage/database.hpp"
#include "storage/sqlite.hpp"
#include <fstream>
#include <stdexcept>
namespace ortho {
using storage::exec;
using storage::open;
using storage::pragma_number;
using storage::Statement;
using storage::Transaction;
PronunciationReviewDb::PronunciationReviewDb(const std::filesystem::path& path) {
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path());
    db_ = open(path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    const int version = pragma_number(db_.get(), "PRAGMA user_version"),
              identity = pragma_number(db_.get(), "PRAGMA application_id");
    if (version > 1 || (identity != 0 && identity != 0x4f525450) || (version == 1 && identity != 0x4f525450))
        throw std::runtime_error("Unsupported pronunciation review database");
    exec(db_.get(), "PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL; PRAGMA cache_size=-256; PRAGMA "
                    "wal_autocheckpoint=64");
    if (!version) {
        Transaction transaction(db_.get());
        exec(db_.get(),
             "CREATE TABLE review(key TEXT PRIMARY KEY NOT NULL,form TEXT NOT NULL,status TEXT NOT NULL "
             "CHECK(status IN ('approved','corrected','deferred')),spoken TEXT NOT NULL,note TEXT NOT "
             "NULL,updated INTEGER NOT NULL,CHECK(status!='corrected' OR length(trim(spoken))>0)) STRICT; "
             "PRAGMA application_id=1330795600; PRAGMA user_version=1");
        transaction.commit();
    }
}
std::vector<PronunciationDecision> PronunciationReviewDb::decisions() const {
    Statement q(db_.get(), "SELECT form,status,spoken,note FROM review ORDER BY key");
    std::vector<PronunciationDecision> result;
    while (q.row())
        result.push_back({q.text(0), q.text(1), q.text(2), q.text(3)});
    return result;
}
void PronunciationReviewDb::save(const PronunciationDecision& value) {
    if (value.form.empty() || value.form.find_first_of("\t\r\n") != std::string::npos)
        throw std::runtime_error("Invalid pronunciation word");
    Transaction transaction(db_.get());
    Statement q(db_.get(), "INSERT INTO review VALUES(?,?,?,?,?,unixepoch()) ON CONFLICT(key) DO UPDATE SET "
                           "form=excluded.form,status=excluded.status,spoken=excluded.spoken,note=excluded."
                           "note,updated=excluded.updated");
    q.text(1, pronunciation_key(value.form));
    q.text(2, value.form);
    q.text(3, value.status);
    q.text(4, value.spoken);
    q.text(5, value.note);
    q.row();
    transaction.commit();
}
std::vector<Pronunciation> PronunciationReviewDb::overrides() const {
    std::vector<Pronunciation> result;
    for (const auto& value : decisions()) {
        if (value.status == "corrected")
            result.push_back({"sv", value.form, value.spoken, "", 1000});
    }
    return result;
}
void PronunciationReviewDb::export_tsv(const std::filesystem::path& path) const {
    const auto clean = [](std::string text) {
        for (char& c : text)
            if (c == '\t' || c == '\r' || c == '\n')
                c = ' ';
        return text;
    };
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        throw std::runtime_error("Cannot open pronunciation export");
    out << "# form\tstatus\tspoken\tnote\n";
    for (const auto& value : decisions())
        out << clean(value.form) << '\t' << value.status << '\t' << clean(value.spoken) << '\t'
            << clean(value.note) << '\n';
    out.close();
    if (!out)
        throw std::runtime_error("Cannot write pronunciation export");
}
} // namespace ortho
