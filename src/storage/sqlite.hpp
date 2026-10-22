#pragma once

#include "storage/database.hpp"
#include <sqlite3.h>

// Internal SQLite primitives. Database policy and SQL belong to each repository.
namespace ortho::storage {
DatabaseHandle open(const std::filesystem::path& path, int flags);
void exec(sqlite3* db, const char* sql);
int pragma_number(sqlite3* db, const char* sql);

class Statement final {
public:
    Statement(sqlite3* db, const char* sql);
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    void text(int index, const std::string& value);
    void number(int index, int value);
    void blob(int index, const std::vector<unsigned char>& value);
    std::vector<unsigned char> blob(int column) const;
    void reset();
    bool row();
    int number(int column) const;
    std::string text(int column) const;

private:
    sqlite3* db_;
    sqlite3_stmt* stmt_ = nullptr;
};

class Transaction final {
public:
    explicit Transaction(sqlite3* db);
    ~Transaction();
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    void commit();

private:
    sqlite3* db_;
    bool committed_ = false;
};
} // namespace ortho::storage
