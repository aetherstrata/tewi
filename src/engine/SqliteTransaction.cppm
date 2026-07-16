module;
#include "common/Log.h"
module tewi;

import :sqlite_transaction;
import :sqlite_connection;
import :logging;

namespace tewi::engine
{
SqliteTransaction::SqliteTransaction(SqliteConnection& db)
    : _db(db)
{
    // SQLite rejects a BEGIN inside an open transaction, so an inner guard opens
    // a savepoint instead. An empty _savepoint marks the outermost guard - the
    // one that owns the real transaction.
    if (_db.inTransaction())
    {
        _savepoint = _db.nextSavepointName();
        _db.exec(std::format("SAVEPOINT {};", _savepoint));
    }
    else
    {
        _db.exec("BEGIN;");
    }
}

SqliteTransaction::~SqliteTransaction() noexcept
{
    if (_committed) return;

    try
    {
        rollback();
    }
    catch (...)
    {
        LOG_ERR("Failed to roll back transaction in destructor. Exception swallowed to avoid "
              "std::terminate().");
    }
}

void SqliteTransaction::commit()
{
    // Releasing a savepoint does not commit anything: it merges this level into
    // the enclosing one, and only the outermost COMMIT reaches the disk.
    if (_savepoint.empty()) _db.exec("COMMIT;");
    else                    _db.exec("RELEASE " + _savepoint + ";");

    _committed = true;
}

void SqliteTransaction::rollback()
{
    if (_savepoint.empty())
    {
        _db.exec("ROLLBACK;");
    }
    else
    {
        // ROLLBACK TO rewinds to the savepoint but leaves it on the stack, so it
        // needs an explicit RELEASE to pop. Without that, the level stays open
        // and the enclosing guard's RELEASE/COMMIT would target the wrong depth.
        _db.exec("ROLLBACK TO " + _savepoint + ";");
        _db.exec("RELEASE " + _savepoint + ";");
    }

    _committed = true;
}
} // namespace tewi::engine
