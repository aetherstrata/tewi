module tewi;

import :sqlite_transaction;
import :sqlite_connection;
import :logging;

namespace tewi::engine
{
SqliteTransaction::SqliteTransaction(SqliteConnection& db)
    : _db(db)
{
    _db.exec("BEGIN;");
}

SqliteTransaction::~SqliteTransaction() noexcept
{
    if (_committed) return;

    try
    {
        _db.exec("ROLLBACK;");
    }
    catch (...)
    {
        error("Failed to roll back transaction in destructor. Exception swallowed to avoid "
              "std::terminate().");
    }
}

void SqliteTransaction::commit()
{
    _db.exec("COMMIT;");
    _committed = true;
}

void SqliteTransaction::rollback()
{
    _db.exec("ROLLBACK;");
    _committed = true;
}
} // namespace tewi::engine
