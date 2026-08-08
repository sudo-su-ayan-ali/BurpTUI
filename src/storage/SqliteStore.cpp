#include "storage/SqliteStore.hpp"
#include <sqlite3.h>
#include <stdexcept>

namespace BurpTUI {

struct SqliteStore::Impl {
    sqlite3* db = nullptr;
};

SqliteStore::SqliteStore(const std::string& dbPath) : impl_(new Impl{}) {
    if (sqlite3_open(dbPath.c_str(), &impl_->db) != SQLITE_OK)
        throw std::runtime_error("Cannot open database: " + dbPath);
    // Create table if not exists
    const char* sql =
        "CREATE TABLE IF NOT EXISTS requests ("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  method      TEXT,"
        "  host        TEXT,"
        "  url         TEXT,"
        "  status_code INTEGER,"
        "  resp_len    INTEGER,"
        "  timestamp   TEXT,"
        "  raw_request BLOB,"
        "  raw_response BLOB"
        ");";
    char* err = nullptr;
    sqlite3_exec(impl_->db, sql, nullptr, nullptr, &err);
    if (err) { sqlite3_free(err); }
}

SqliteStore::~SqliteStore() {
    if (impl_) { sqlite3_close(impl_->db); delete impl_; }
}

void SqliteStore::save(const ProxyEntry& /*entry*/) {
    // TODO: implement in storage phase
}

std::vector<ProxyEntry> SqliteStore::list() const {
    return {}; // TODO
}

std::optional<ProxyEntry> SqliteStore::get(std::uint64_t /*id*/) const {
    return std::nullopt; // TODO
}

void SqliteStore::clear() {
    sqlite3_exec(impl_->db, "DELETE FROM requests;", nullptr, nullptr, nullptr);
}

} // namespace BurpTUI
