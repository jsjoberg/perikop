#include "speech/speech.hpp"
#include "storage/database.hpp"
#include "storage/sqlite.hpp"
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <utility>
namespace ortho {
namespace {
// Corrections saved before version 2 wrote ⟦…⟧ in IPA. The same codes the voice used then.
constexpr std::pair<std::string_view, std::string_view> legacy_ipa[] = {
    {"iː", "i:"}, {"i", "i"},   {"ɪ", "I"},  {"eː", "e:"}, {"e", "e"},   {"ɛː", "E:"}, {"ɛ", "E"},
    {"ə", "E-"},  {"yː", "y:"}, {"y", "y"},  {"ʏ", "y"},   {"ʉː", "u-"}, {"ʉ", "u-"},  {"ɨː", "u-"},
    {"ɵ", "8"},   {"øː", "Y:"}, {"ø", "Y:"}, {"œː", "W"},  {"œ", "W"},   {"æː", "&:"}, {"æ", "&"},
    {"ɑː", "A:"}, {"ɑ", "A:"},  {"a", "a"},  {"oː", "o:"}, {"o", "o"},   {"ɔ", "O"},   {"uː", "u:"},
    {"u", "u"},   {"ʊ", "U"},   {"p", "p"},  {"b", "b"},   {"t", "t"},   {"d", "d"},   {"k", "k"},
    {"ɡ", "g"},   {"m", "m"},   {"n", "n"},  {"ŋ", "N"},   {"f", "f"},   {"v", "v"},   {"s", "s"},
    {"h", "h"},   {"l", "l"},   {"j", "j"},  {"r", "R"},   {"ɕ", "S;"},  {"ɧ", "sx"},  {"ʂ", "s."},
    {"ʈ", "Rt"},  {"ɖ", "Rd"},  {"ɳ", "n."}, {"ɭ", "l."},  {"ʧ", "tS"},  {"ˈ", "'"},   {"ˌ", ","},
};
// eSpeak codes for one ⟦…⟧ of IPA, read as speech read it then: without spaces, and dropping
// symbols that had no code.
std::string legacy_phonemes(std::string_view text) {
    std::string ipa;
    for (char c : text)
        if (c != ' ')
            ipa += c;
    std::string result;
    for (size_t at = 0; at < ipa.size();) {
        const auto lead = static_cast<unsigned char>(ipa[at]);
        size_t size = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
        if (ipa.substr(at + size).starts_with("ː"))
            size += std::string_view("ː").size();
        const auto symbol = ipa.substr(at, size);
        at += size;
        for (const auto& [from, to] : legacy_ipa)
            if (symbol == from) {
                result += to;
                break;
            }
    }
    return result;
}
std::string legacy_spoken(const std::string& spoken) {
    std::string result;
    size_t at = 0;
    for (auto open = spoken.find("⟦"); open != std::string::npos; open = spoken.find("⟦", at)) {
        const auto begin = open + std::string_view("⟦").size(), close = spoken.find("⟧", begin);
        if (close == std::string::npos)
            break;
        result += spoken.substr(at, begin - at) +
                  legacy_phonemes(std::string_view(spoken).substr(begin, close - begin));
        at = close;
    }
    return result + spoken.substr(at);
}
} // namespace
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
    if (version > 2 || (identity != 0 && identity != 0x4f525450) || (version >= 1 && identity != 0x4f525450))
        throw std::runtime_error("Unsupported pronunciation review database");
    exec(db_.get(), "PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL; PRAGMA cache_size=-256; PRAGMA "
                    "wal_autocheckpoint=64");
    if (!version) {
        Transaction transaction(db_.get());
        exec(db_.get(),
             "CREATE TABLE review(key TEXT PRIMARY KEY NOT NULL,form TEXT NOT NULL,status TEXT NOT NULL "
             "CHECK(status IN ('approved','corrected','deferred')),spoken TEXT NOT NULL,note TEXT NOT "
             "NULL,updated INTEGER NOT NULL,CHECK(status!='corrected' OR length(trim(spoken))>0)) STRICT; "
             "PRAGMA application_id=1330795600; PRAGMA user_version=2");
        transaction.commit();
    } else if (version == 1) {
        Transaction transaction(db_.get());
        std::vector<std::pair<std::string, std::string>> changed;
        {
            Statement q(db_.get(), "SELECT key,spoken FROM review WHERE instr(spoken,'⟦')");
            while (q.row())
                changed.emplace_back(q.text(0), legacy_spoken(q.text(1)));
        }
        Statement update(db_.get(), "UPDATE review SET spoken=? WHERE key=?");
        for (const auto& [key, spoken] : changed) {
            update.text(1, spoken);
            update.text(2, key);
            update.row();
            update.reset();
        }
        exec(db_.get(), "PRAGMA user_version=2");
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
