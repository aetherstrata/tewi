module;
#include "common/Log.h"
#include <sqlite3.h>
module tewi;

import :sqlite_statement;

import std;

/**
 * @brief Throws `SqliteError` if @p rc does not equal @p ok_sentinel.
 *
 * @param[in] rc          SQLite result code to inspect.
 * @param[in] context     Short description of the operation, included in the
 *                        exception message on failure.
 * @param[in] ok_sentinel The result code treated as success. Defaults to
 *                        `SQLITE_OK`.
 *
 * @throws SqliteError when `rc != ok_sentinel`.
 */
static void check(const tewi::i32 rc, std::string_view context, tewi::i32 ok_sentinel = SQLITE_OK)
{
    if (rc != ok_sentinel)
    {
        LOG_ERR("SQLite operation failed: {} {}", context,
                tewi::engine::SqliteError::PrettyCode(rc));
        throw tewi::engine::SqliteError(std::string(context), rc);
    }
}

namespace tewi::engine
{
SqliteStatement::SqliteStatement(sqlite3* db, std::string_view sql)
{
    sqlite3_stmt* raw = nullptr;
    const i32 rc = sqlite3_prepare_v2(db, sql.data(), static_cast<i32>(sql.size()), &raw, nullptr);
    _stmt.reset(raw);
    check(rc, "prepare_v2");
}

SqliteStatement& SqliteStatement::bind(const i32 idx, const i32 val)
{
    check(sqlite3_bind_int(_stmt.get(), idx, val), "bind i32");
    LOG_TRACE("Bound i32 value {} at index {}", val, idx);
    return *this;
}

SqliteStatement& SqliteStatement::bind(const i32 idx, const i64 val)
{
    check(sqlite3_bind_int64(_stmt.get(), idx, val), "bind i64");
    LOG_TRACE("Bound i64 value {} at index {}", val, idx);
    return *this;
}

SqliteStatement& SqliteStatement::bind(const i32 idx, const f64 val)
{
    check(sqlite3_bind_double(_stmt.get(), idx, val), "bind f64");
    LOG_TRACE("Bound f64 value {} at index {}", val, idx);
    return *this;
}

SqliteStatement& SqliteStatement::bind(const i32 idx, const std::string_view val)
{
    check(sqlite3_bind_text(_stmt.get(), idx, val.data(), static_cast<i32>(val.size()),
                            SQLITE_TRANSIENT),
          "bind text");
    LOG_TRACE("Bound text value \"{}\" at index {}", val, idx);
    return *this;
}

SqliteStatement& SqliteStatement::bind(const i32 idx, const std::span<const std::byte> val)
{
    check(sqlite3_bind_blob(_stmt.get(), idx, val.data(), static_cast<i32>(val.size_bytes()),
                            SQLITE_TRANSIENT),
          "bind blob");
    LOG_TRACE("Bound blob value of {} bytes at index {}", val.size_bytes(), idx);
    return *this;
}

SqliteStatement& SqliteStatement::bind(const i32 idx, std::nullptr_t)
{
    check(sqlite3_bind_null(_stmt.get(), idx), "bind null");
    LOG_TRACE("Bound NULL value at index {}", idx);
    return *this;
}

bool SqliteStatement::step()
{
    const i32 rc = sqlite3_step(_stmt.get());
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;

    LOG_ERR("step() failed while stepping statement {}", SqliteError::PrettyCode(rc));
    throw SqliteError("step() failed", rc);
}

void SqliteStatement::exec()
{
    while (step());
    reset();
}

void SqliteStatement::reset() noexcept
{
    sqlite3_reset(_stmt.get());
}

void SqliteStatement::clearBindings() noexcept
{
    sqlite3_clear_bindings(_stmt.get());
}

i32 SqliteStatement::columnInt(const i32 col) const
{
    return sqlite3_column_int(_stmt.get(), col);
}

i64 SqliteStatement::columnLong(const i32 col) const
{
    return sqlite3_column_int64(_stmt.get(), col);
}

f64 SqliteStatement::columnDouble(const i32 col) const
{
    return sqlite3_column_double(_stmt.get(), col);
}

std::string SqliteStatement::columnText(const i32 col) const
{
    const auto* raw = sqlite3_column_text(_stmt.get(), col);
    return raw != nullptr ? reinterpret_cast<const char*>(raw) : "";
}

std::span<const std::byte> SqliteStatement::columnBlob(const i32 col) const
{
    const void* ptr   = sqlite3_column_blob(_stmt.get(), col);
    const usize bytes = sqlite3_column_bytes(_stmt.get(), col);

    if (ptr == nullptr || bytes <= 0) return {};

    return {static_cast<const std::byte*>(ptr), bytes};
}

bool SqliteStatement::columnIsNull(const i32 col) const
{
    return sqlite3_column_type(_stmt.get(), col) == SQLITE_NULL;
}

i32 SqliteStatement::columnCount() const
{
    return sqlite3_column_count(_stmt.get());
}

std::string SqliteStatement::columnName(const i32 col) const
{
    return sqlite3_column_name(_stmt.get(), col);
}

sqlite3_stmt* SqliteStatement::handle() const noexcept
{
    return _stmt.get();
}

i32 SqliteStatement::getIndexFromName(std::string_view name)
{
    if (const auto& it = _paramIndexCache.find(name); it != _paramIndexCache.end())
    {
        return it->second;
    }

    i32 idx = sqlite3_bind_parameter_index(_stmt.get(), std::string(name).c_str());
    if (idx == 0) throw SqliteError("Unknown bind parameter: " + std::string(name));
    LOG_TRACE("Parameter {} has index {}", name, idx);
    _paramIndexCache.emplace(std::string(name), idx);

    return idx;
}
} // namespace tewi::engine
