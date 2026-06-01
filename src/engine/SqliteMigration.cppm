module tewi;

import :sqlite_connection;
import :sqlite_statement;
import :sqlite_transaction;
import :sqlite_migration;

import std;

namespace tewi::engine
{
static constexpr std::array migrations{
    Migration{.version = 1, .sql = R"(
        CREATE TABLE test_cases (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            family_id     INTEGER NOT NULL,
            name          TEXT    NOT NULL,
            description   TEXT    NOT NULL DEFAULT '',
            last_run_date TEXT,
            test_input    TEXT    NOT NULL DEFAULT '',
            test_output   TEXT    NOT NULL DEFAULT '',
            status        TEXT    NOT NULL DEFAULT 'not_executed'
        );
    )"},
};
void runMigrations(SqliteConnection& database)
{
    int dbVersion = 0;
    if (auto stmt = database.prepare("PRAGMA user_version;"); stmt.step())
    {
        dbVersion = stmt.columnInt(0);
    }

    if (dbVersion <= 0) throw SqliteError("Failed to read database version");

    debug(std::format("Current database version: {}", dbVersion));
    debug("Checking for migrations...");

    for (const auto& migration : migrations)
    {
        // Nothing to do if the database is already at or above this version
        if (migration.version <= dbVersion) continue;

        info(std::format("Applying DB migration to version {}...", migration.version));

        // Each migration is atomic. Partial application is never committed
        auto txn = database.beginTransaction();
        database.exec(migration.sql);

        // Update the version inside the same transaction
        database.exec(std::format("PRAGMA user_version = {};", migration.version));

        txn.commit();
    }
}
} // namespace tewi::engine
