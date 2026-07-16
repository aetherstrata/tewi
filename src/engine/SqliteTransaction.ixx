module tewi:sqlite_transaction;

namespace tewi::engine
{
class SqliteConnection;

/**
 * @class SqliteTransaction
 * @brief RAII guard that wraps a SQLite @c BEGIN / @c COMMIT / @c 0ROLLBACK block.
 *
 * @details Executes @c BEGIN; in the constructor. If @c Commit() is never called
 * before the guard is destroyed (e.g. because an exception is thrown), the
 * destructor issues @c ROLLBACK;, guaranteeing that partial work is never
 * silently committed.
 *
 * @par Usage
 * @code{.cpp}
 *   {
 *       auto txn = db.beginTransaction();
 *       db.prepare("INSERT INTO ...").bind(...).exec();
 *       db.prepare("UPDATE ...").bind(...).exec();
 *       txn.commit(); // omit this to roll back automatically
 *   }
 * @endcode
 *
 * @note Nested transactions are not supported by this class. Use SQLite
 *       @c SAVEPOINT commands directly via @c SqliteDatabase::exec() if
 *       nesting is required.
 */
class SqliteTransaction
{
public:
    /**
     * @brief Begins a transaction on the given database.
     *
     * @param[in] db  Reference to an open @c SqliteDatabase. The reference must remain
     *                valid for the lifetime of this @c SqliteTransaction.
     *
     * @throws SqliteError if @c BEGIN fails.
     */
    explicit SqliteTransaction(SqliteConnection& db);

    /**
     * @brief Rolls back the transaction if it was not committed.
     *
     * @details Issues @c ROLLBACK; in a best-effort, non-throwing manner.
     * Any error from @c ROLLBACK is silently swallowed because throwing from a
     * destructor during stack unwinding would call @c std::terminate().
     */
    ~SqliteTransaction() noexcept;

    /// @brief Deleted copy constructor - transactions are not copyable.
    SqliteTransaction(const SqliteTransaction&) = delete;
    /// @brief Deleted copy-assignment - transactions are not copyable.
    SqliteTransaction& operator=(const SqliteTransaction&) = delete;
    /// @brief Deleted move constructor - transactions are not movable.
    SqliteTransaction(const SqliteTransaction&&) = delete;
    /// @brief Deleted move-assignment - transactions are not movable.
    SqliteTransaction& operator=(const SqliteTransaction&&) = delete;
    /**
     * @brief Commits the transaction, making all changes permanent.
     *
     * @details Executes @c COMMIT; and marks the guard as committed so that
     * the destructor will not roll back.
     *
     * @throws SqliteError if the commit fails (e.g. I/O error, constraint
     *         violation from a deferred check).
     */
    void commit();

    /**
     * @brief Explicitly rolls back the transaction before the guard goes out of scope.
     *
     * @details Executes @c ROLLBACK; and marks the guard as "committed" (i.e.
     * resolved) so that the destructor does not attempt a second rollback.
     * Useful when rollback is a deliberate, non-exceptional outcome.
     *
     * @throws SqliteError if the rollback fails.
     */
    void rollback();

private:
    SqliteConnection& _db;     ///< Reference to the owning database connection.
    bool _committed = false; ///< Set to @c true once @c Commit() or @c Rollback() has been called.
};
} // namespace tewi::engine
