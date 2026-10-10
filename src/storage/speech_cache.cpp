#include "storage/database.hpp"
#include "storage/sqlite.hpp"
#include <bit>
#include <cmath>
#include <stdexcept>
namespace ortho {
using storage::exec;
using storage::open;
using storage::pragma_number;
using storage::Statement;
using storage::Transaction;
SpeechCache::SpeechCache(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    db_ = open(path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    exec(db_.get(), "PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL; PRAGMA foreign_keys=ON; PRAGMA "
                    "cache_size=-2048; PRAGMA wal_autocheckpoint=256");
    const int version = pragma_number(db_.get(), "PRAGMA user_version");
    const int identity = pragma_number(db_.get(), "PRAGMA application_id");
    if (version > 1 || (identity != 0 && identity != 0x4f525453))
        throw std::runtime_error("Unsupported speech cache");
    if (!version) {
        Transaction transaction(db_.get());
        exec(
            db_.get(),
            "CREATE TABLE audio(model TEXT NOT NULL,language TEXT NOT NULL,text TEXT NOT NULL,pcm_f32le BLOB "
            "NOT NULL CHECK(length(pcm_f32le)>0 AND length(pcm_f32le)%4=0),used INTEGER NOT NULL,PRIMARY "
            "KEY(model,language,text)) STRICT; PRAGMA application_id=1330795603; PRAGMA user_version=1");
        transaction.commit();
    } else if (identity != 0x4f525453)
        throw std::runtime_error("Unsupported speech cache identity");
}
bool SpeechCache::contains(const std::string& model, const std::string& language, const std::string& text) {
    Statement query(db_.get(), "SELECT length(pcm_f32le) FROM audio WHERE model=? AND language=? AND text=?");
    query.text(1, model);
    query.text(2, language);
    query.text(3, text);
    if (!query.row())
        return false;
    const auto bytes = query.number(0);
    return bytes > 0 && bytes % 4 == 0 && bytes <= 16 * 1024 * 1024;
}
std::optional<std::vector<float>> SpeechCache::load(const std::string& model, const std::string& language,
                                                    const std::string& text) {
    Statement query(db_.get(), "SELECT pcm_f32le FROM audio WHERE model=? AND language=? AND text=?");
    query.text(1, model);
    query.text(2, language);
    query.text(3, text);
    if (!query.row())
        return {};
    const auto bytes = query.blob(0);
    if (bytes.empty() || bytes.size() % 4 || bytes.size() > size_t{16} * 1024 * 1024)
        return {};
    std::vector<float> samples;
    samples.reserve(bytes.size() / 4);
    for (size_t i = 0; i < bytes.size(); i += 4) {
        const uint32_t bits = uint32_t(bytes[i]) | (uint32_t(bytes[i + 1]) << 8) |
                              (uint32_t(bytes[i + 2]) << 16) | (uint32_t(bytes[i + 3]) << 24);
        const float sample = std::bit_cast<float>(bits);
        if (!std::isfinite(sample))
            return {};
        samples.push_back(sample);
    }
    Statement touch(db_.get(), "UPDATE audio SET used=unixepoch() WHERE model=? AND language=? AND text=?");
    touch.text(1, model);
    touch.text(2, language);
    touch.text(3, text);
    touch.row();
    return samples;
}
void SpeechCache::save(const std::string& model, const std::string& language, const std::string& text,
                       const std::vector<float>& samples) {
    if (samples.empty() || samples.size() > size_t{4} * 1024 * 1024)
        throw std::runtime_error("Invalid speech audio length");
    std::vector<unsigned char> bytes;
    bytes.reserve(samples.size() * 4);
    for (const float sample : samples) {
        if (!std::isfinite(sample))
            throw std::runtime_error("Invalid speech audio sample");
        const auto bits = std::bit_cast<uint32_t>(sample);
        for (int shift = 0; shift < 32; shift += 8)
            bytes.push_back(static_cast<unsigned char>(bits >> shift));
    }
    Transaction transaction(db_.get());
    Statement insert(db_.get(),
                     "INSERT INTO audio VALUES(?,?,?,?,unixepoch()) ON CONFLICT(model,language,text) DO "
                     "UPDATE SET pcm_f32le=excluded.pcm_f32le,used=excluded.used");
    insert.text(1, model);
    insert.text(2, language);
    insert.text(3, text);
    insert.blob(4, bytes);
    insert.row();
    // At most 256 MiB of audio. SQLite reuses evicted pages for future entries.
    while (pragma_number(db_.get(), "SELECT coalesce(sum(length(pcm_f32le)),0) FROM audio") >
           256 * 1024 * 1024)
        exec(db_.get(), "DELETE FROM audio WHERE (model,language,text)=(SELECT model,language,text FROM "
                        "audio ORDER BY used,model,language,text LIMIT 1)");
    transaction.commit();
}
} // namespace ortho
