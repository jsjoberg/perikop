#include "storage/sqlite.hpp"
#include <stdexcept>
namespace ortho {
void SqliteCloser::operator()(sqlite3* db) const {
    if (db)
        sqlite3_close(db);
}
} // namespace ortho
namespace ortho::storage {
namespace {
std::string utf8_path(const std::filesystem::path& path) {
    auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}
} // namespace
DatabaseHandle open(const std::filesystem::path& path, int flags) {
    sqlite3* raw = nullptr;
    const int status = sqlite3_open_v2(utf8_path(path).c_str(), &raw, flags, nullptr);
    DatabaseHandle result{raw};
    if (status != SQLITE_OK)
        throw std::runtime_error("Cannot open database: " +
                                 std::string(raw ? sqlite3_errmsg(raw) : "out of memory"));
    sqlite3_extended_result_codes(raw, 1);
    if (sqlite3_busy_timeout(raw, 3000) != SQLITE_OK ||
        sqlite3_db_config(raw, SQLITE_DBCONFIG_DEFENSIVE, 1, nullptr) != SQLITE_OK ||
        sqlite3_db_config(raw, SQLITE_DBCONFIG_TRUSTED_SCHEMA, 0, nullptr) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(raw));
    return result;
}
Statement::Statement(sqlite3* db, const char* sql) : db_(db) {
    if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK) {
        sqlite3_finalize(stmt_);
        throw std::runtime_error(sqlite3_errmsg(db));
    }
}
Statement::~Statement() {
    sqlite3_finalize(stmt_);
}
void Statement::text(int index, const std::string& value) {
    if (sqlite3_bind_text64(stmt_, index, value.data(), value.size(), SQLITE_TRANSIENT, SQLITE_UTF8) !=
        SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(db_));
}
void Statement::number(int index, int value) {
    if (sqlite3_bind_int(stmt_, index, value) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(db_));
}
void Statement::blob(int index, const std::vector<unsigned char>& value) {
    if (sqlite3_bind_blob64(stmt_, index, value.data(), value.size(), SQLITE_TRANSIENT) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(db_));
}
std::vector<unsigned char> Statement::blob(int column) const {
    const auto* bytes = static_cast<const unsigned char*>(sqlite3_column_blob(stmt_, column));
    const int count = sqlite3_column_bytes(stmt_, column);
    return bytes && count ? std::vector<unsigned char>(bytes, bytes + count) : std::vector<unsigned char>{};
}
void Statement::reset() {
    if (sqlite3_reset(stmt_) != SQLITE_OK || sqlite3_clear_bindings(stmt_) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(db_));
}
bool Statement::row() {
    const int status = sqlite3_step(stmt_);
    if (status == SQLITE_ROW)
        return true;
    if (status == SQLITE_DONE)
        return false;
    throw std::runtime_error(sqlite3_errmsg(db_));
}
int Statement::number(int column) const {
    return sqlite3_column_int(stmt_, column);
}
std::string Statement::text(int column) const {
    const auto* value = sqlite3_column_text(stmt_, column);
    return value ? std::string(reinterpret_cast<const char*>(value), sqlite3_column_bytes(stmt_, column))
                 : "";
}
void exec(sqlite3* db, const char* sql) {
    if (sqlite3_exec(db, sql, nullptr, nullptr, nullptr) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(db));
}
int pragma_number(sqlite3* db, const char* sql) {
    Statement query(db, sql);
    if (!query.row())
        throw std::runtime_error("Missing database metadata");
    return query.number(0);
}
Transaction::Transaction(sqlite3* db) : db_(db) {
    exec(db_, "BEGIN IMMEDIATE");
}
Transaction::~Transaction() {
    if (!committed_)
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
}
void Transaction::commit() {
    exec(db_, "COMMIT");
    committed_ = true;
}
} // namespace ortho::storage
