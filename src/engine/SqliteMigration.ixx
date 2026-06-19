export module tewi:sqlite_migration;

import :sqlite_connection;
import std;

namespace tewi::engine
{
/**
 * @struct Migration
 * @brief Represents a single, immutable schema migration step.
 *
 * @details Each migration has a monotonically increasing version number and
 * a SQL string that is executed inside a transaction. Once a migration has
 * been applied to a database it must never be modified, as this would break
 * the migration history and potentially cause data loss. New migrations
 * should be appended at the end instead.
 */
export struct Migration
{
    int version{};          ///< Target schema version after this migration runs.
    std::string_view sql{}; ///< DDL/DML to execute. May contain multiple statements.
};

/**
 * @brief Applies all pending migrations to a database in order.
 *
 * @details Reads `PRAGMA user_version` to determine the current schema
 * version, then applies every @c Migration whose @c version exceeds it.
 * Each migration runs inside its own transaction. If one fails, it is
 * rolled back and an exception is thrown, leaving the database at the
 * last successfully applied version.
 *
 * After each successful migration `PRAGMA user_version` is updated so
 * that the next run skips already-applied steps.
 *
 * @param[in] database  Open database to migrate.
 * @param[in] migrations The migrations to apply to the database
 *
 * @throws SqliteError if any migration SQL fails.
 */
export void runMigrations(SqliteConnection& database, std::span<const Migration> migration);
} // namespace tewi::engine
