module;
#include <sqlite3.h>
#include "common/Log.h"
module tewi;

import std;

namespace tewi::engine
{
SqliteConnection::SqliteConnection(std::string_view path, OpenMode mode)
    : _path(path)
{
    LOG_INFO("Opening database at path: {}", path);

    sqlite3* raw = nullptr;
    // _path, not path: string_view::data() carries no NUL-termination guarantee.
    const int resultCode = sqlite3_open_v2(_path.c_str(), &raw, std::to_underlying(mode), nullptr);
    _db.reset(raw);
    if (resultCode != SQLITE_OK)
    {
        LOG_CRITICAL("Failed to open database: {} {}", path, SqliteError::PrettyCode(resultCode));
        throw SqliteError("Failed to open database: " + std::string(path), resultCode);
    }

    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA foreign_keys=ON;");
}

void SqliteConnection::exec(std::string_view sql)
{
    LOG_DBG("Executing SQL statement: {}", sql);

    // sql.data() carries no NUL-termination guarantee; sqlite3_exec needs one.
    const std::string sqlText(sql);

    char* errmsg = nullptr;
    const int rc = sqlite3_exec(handle(), sqlText.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK)
    {
        const std::string msg = errmsg != nullptr ? errmsg : "(no message)";
        sqlite3_free(errmsg);
        LOG_ERR("SQL execution failed: {} {}", msg, SqliteError::PrettyCode(rc));
        throw SqliteError("exec() failed: " + msg, rc);
    }
}

SqliteStatement SqliteConnection::prepare(std::string_view sql)
{
    LOG_DBG("Preparing SQL statement: {}", sql);
    return SqliteStatement{handle(), sql};
}

SqliteTransaction SqliteConnection::beginTransaction()
{
    return SqliteTransaction(*this);
}

int SqliteConnection::changes() const noexcept
{
    return sqlite3_changes(_db.get());
}

i64 SqliteConnection::lastInsertRowId() const noexcept
{
    return sqlite3_last_insert_rowid(_db.get());
}

i32 SqliteConnection::schemaVersion() const
{
    i32 dbVersion = 0;
    if (auto stmt = const_cast<SqliteConnection*>(this)->prepare("PRAGMA user_version;"); stmt.step())
    {
        dbVersion = stmt.columnInt(0);
    }

    if (dbVersion <= 0) throw SqliteError("Failed to read database version");

    return dbVersion;
}
} // namespace tewi::engine
