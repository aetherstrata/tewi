module;
#include "common/Log.h"
module tewi;

import :sqlite_connection;
import :sqlite_statement;
import :sqlite_transaction;
import :sqlite_migration;

import std;

namespace tewi::engine
{
/**
 * @brief Returns a vector of indices representing the migrations sorted by ascending version.
 * 
 * @param migrations A span of Migration objects.
 * @return std::vector<usize> The sorted indices.
 */
std::vector<usize> getSortedMigrationIndices(std::span<const Migration> migrations)
{
    std::vector<usize> indices(migrations.size());

    //  0, 1, 2, ...
    std::iota(indices.begin(), indices.end(), 0);

    // Sort the indices based on the 'version' field of the corresponding Migration objects
    std::ranges::sort(indices, [&migrations](usize a, usize b)
    {
        return migrations[a].version < migrations[b].version;
    });

    return indices;
}

void runMigrations(SqliteConnection& database, const std::span<const Migration> migrations)
{
    int dbVersion = database.schemaVersion();

    LOG_DBG("Current database version: {}", dbVersion);
    LOG_DBG("Checking for migrations...");

    for (const usize i : getSortedMigrationIndices(migrations))
    {
        const auto& [version, sql] = migrations[i];

        // Nothing to do if the database is already at or above this version
        if (version <= dbVersion) continue;

        LOG_INFO("Applying DB migration to version {}...", version);

        // Each migration is atomic. Partial application is never committed
        auto txn = database.beginTransaction();
        database.exec(sql);

        // Update the version inside the same transaction
        database.exec(std::format("PRAGMA user_version = {};", version));

        txn.commit();
    }
}
} // namespace tewi::engine
