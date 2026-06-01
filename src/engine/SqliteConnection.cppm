module;
#include <sqlite3.h>
#include <utility> // Apparently std::to_underlying is not part of the module?
module tewi;

import std;

namespace tewi::engine
{
SqliteConnection::SqliteConnection(std::string_view path, OpenMode mode)
    : _path(path)
{
    info(std::format("Opening database at path: {}", path));

    sqlite3* raw         = nullptr;
    const int resultCode = sqlite3_open_v2(path.data(), &raw, std::to_underlying(mode), nullptr);
    _db.reset(raw);
    if (resultCode != SQLITE_OK)
    {
        error(std::format("Failed to open database: {} {}", path,
                          SqliteError::PrettyCode(resultCode)));
        throw SqliteError("Failed to open database: " + std::string(path), resultCode);
    }

    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA foreign_keys=ON;");
}

void SqliteConnection::exec(std::string_view sql)
{
    debug(std::format("Executing SQL query: {}", sql));

    char* errmsg = nullptr;
    const int rc = sqlite3_exec(Handle(), sql.data(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK)
    {
        const std::string msg = errmsg != nullptr ? errmsg : "(no message)";
        sqlite3_free(errmsg);
        log(LogLevel::Error,
            std::format("SQL execution failed: {} {}", msg, SqliteError::PrettyCode(rc)));
        throw SqliteError("exec() failed: " + msg, rc);
    }
}

SqliteStatement SqliteConnection::prepare(std::string_view sql)
{
    debug(std::format("Preparing SQL statement: {}", sql));
    return SqliteStatement{Handle(), sql};
}

SqliteTransaction SqliteConnection::beginTransaction()
{
    return SqliteTransaction(*this);
}

int SqliteConnection::Changes() const noexcept
{
    return sqlite3_changes(_db.get());
}

i64 SqliteConnection::lastInsertRowId() const noexcept
{
    return sqlite3_last_insert_rowid(_db.get());
}
} // namespace tewi::engine
