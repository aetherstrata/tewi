module;
#include <sqlite3.h>
export module tewi:sqlite_connection;

import :number_types;

import std;

namespace tewi::engine
{
class SqliteStatement;
class SqliteTransaction;

/**
 * @class SqliteConnection
 * @brief RAII owner of a @c sqlite3* database connection.
 *
 * @details Opens a connection to the database on construction and closes it
 * on destruction, even if the destructor runs due to stack unwinding. The
 * class is move-only; copying a database handle is intentionally disallowed.
 *
 * The constructor automatically enables WAL journal mode and foreign-key
 * enforcement, which are strongly recommended defaults for application use.
 *
 * @par Thread safety
 * A single @c SqliteDatabase instance must not be used concurrently from multiple
 * threads without external synchronization. Create one SqliteDatabase per thread,
 * or wrap access in a mutex.
 */
export class SqliteConnection
{
public:
    /**
     * @brief Flags that control how the database file is opened.
     *
     * @details Maps directly to the corresponding @c SQLITE_OPEN_* flags
     * accepted by @c sqlite3_open_v2().
     */
    enum class OpenMode {
        /// Open existing file for reading only.
        ReadOnly = SQLITE_OPEN_READONLY,
        /// Open existing file for reading and writing.
        ReadWrite = SQLITE_OPEN_READWRITE,
        /// Open for reading and writing, creating the file if absent (default).
        ReadWriteCreate = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
    };

    /**
     * @brief Opens a SQLite database at the given file path.
     *
     * @details Uses @c sqlite3_open_v2() for full control over open flags. On
     * success, immediately executes:
     * - `PRAGMA journal_mode=WAL;` - improves concurrent read throughput.
     * - `PRAGMA foreign_keys=ON;` - enforces @c REFERENCES constraints.
     *
     * @param[in] path  Filesystem path to the database file. Pass @c ":memory:"
     *                  for a transient in-memory database.
     * @param[in] mode  One of the @c OpenMode enumerators. Defaults to
     *                  @c OpenMode::ReadWriteCreate.
     *
     * @throws SqliteError if @c sqlite3_open_v2() returns a non-OK code, or if
     *         either of the initial @c PRAGMA statements fails.
     */
    explicit SqliteConnection(std::string_view path, OpenMode mode = OpenMode::ReadWriteCreate);

    /// @brief Deleted copy constructor. Database connections are not copyable.
    SqliteConnection(const SqliteConnection&) = delete;
    /// @brief Deleted copy-assignment. Database connections are not copyable.
    SqliteConnection& operator=(const SqliteConnection&) = delete;
    /// @brief Move constructor. Transfers ownership of the @c sqlite3* handle.
    SqliteConnection(SqliteConnection&&) = default;
    /// @brief Move-assignment operator. Transfers ownership of the @c sqlite3* handle.
    SqliteConnection& operator=(SqliteConnection&&) = default;

    /**
     * @brief Closes the database connection.
     *
     * @details When the @c SqliteDatabase object goes out of scope or is explicitly
     * destroyed, the underlying @c std::unique_ptr member
     * (@c db_) is automatically destroyed, invoking @c Closer::operator() which
     * calls @c sqlite3_close_v2().
     *
     * @c sqlite3_close_v2() is used rather than @c sqlite3_close() so that any
     * @c sqlite3_stmt* handles that have not yet been finalized do not cause an
     * immediate @c SQLITE_BUSY error. Instead, the connection is placed in a
     * "zombie" state and fully released once the last outstanding statement is
     * finalized.
     *
     * @note Because all resource cleanup is delegated to @c db_'s destructor,
     *       this destructor is @c noexcept by default and will never throw.
     *
     * @warning Destroying a @c SqliteDatabase while a @c SqliteStatement or
     *          @c SqliteTransaction derived from it is still alive results in
     *          undefined behaviour. Always ensure that dependent @c SqliteStatement and
     *          @c SqliteTransaction objects are destroyed (or go out of scope) @b before
     *          the owning @c SqliteDatabase.
     */
    ~SqliteConnection() = default;

    /**
     * @brief Get the database file path
     *
     * @return The filesystem path to the database file
     */
    [[nodiscard]] std::filesystem::path GetPath() const noexcept { return _path; };

    /**
     * @brief Executes one or more SQL statements that return no rows.
     *
     * @details Calls @c sqlite3_exec() internally, which handles multi-statement
     * strings separated by semicolons. Suitable for DDL (`CREATE TABLE`,
     * `DROP INDEX`, …) and `PRAGMA` statements. For parameterised DML use
     * @c prepare() instead.
     *
     * @param[in] sql  Null-terminated or @c string_view compatible SQL string.
     *
     * @throws SqliteError on any SQLite error, with the error message from
     *         @c sqlite3_errmsg() included in the exception text.
     */
    void exec(std::string_view sql);

    /**
     * @brief Compiles a SQL statement into a reusable @c SqliteStatement object.
     *
     * @details Calls @c sqlite3_prepare_v2(). The returned @c SqliteStatement must not
     * outlive this @c SqliteDatabase instance.
     *
     * @param[in] sql  The SQL statement to compile. Must be a single statement
     *                 (no semicolon-separated chains).
     *
     * @return A @c SqliteStatement RAII object that owns the @c sqlite3_stmt* handle.
     *
     * @throws SqliteError if compilation fails (syntax error, unknown table, etc.).
     */
    [[nodiscard]] SqliteStatement prepare(std::string_view sql);

    /**
     * @brief Begins a scoped database transaction.
     *
     * @details Executes @c BEGIN; immediately. The returned @c SqliteTransaction object
     * will automatically issue @c ROLLBACK; in its destructor if @c commit() was
     * never called, providing strong exception safety for multi-statement
     * operations.
     *
     * @return A @c SqliteTransaction RAII guard bound to this database.
     *
     * @throws SqliteError if @c BEGIN fails (e.g. nested explicit transactions).
     */
    [[nodiscard]] SqliteTransaction beginTransaction();

    /**
     * @brief Returns the raw @c sqlite3* handle for low-level SQLite API calls.
     *
     * @warning The pointer remains valid only for the lifetime of this
     *          @c SqliteDatabase object. Do not close or free it externally.
     *
     * @return Non-owning pointer to the underlying @c sqlite3 connection.
     */
    [[nodiscard]] sqlite3* handle() const noexcept { return _db.get(); }

    /**
     * @brief Returns the number of rows modified by the most recent DML statement.
     *
     * @details Wraps @c sqlite3_changes(). The count is reset to zero before
     * each @c INSERT, @c UPDATE, or @c DELETE.
     *
     * @return Row-change count as a non-negative integer.
     */
    [[nodiscard]] int changes() const noexcept;

    /**
     * @brief Returns the row ID inserted by the most recent successful @c INSERT.
     *
     * @details Wraps @c sqlite3_last_insert_rowid(). Returns 0 if no successful
     * @c INSERT has been performed on this connection.
     *
     * @return The 64-bit signed row ID.
     */
    [[nodiscard]] i64 lastInsertRowId() const noexcept;

    /**
     * @brief Get the schema version for this database.
     *
     * @return Returns the current schema version of the database, as set by `PRAGMA user_version`.
     */
    [[nodiscard]] i32 schemaVersion() const;

private:
    /**
     * @brief Custom deleter for the @c unique_ptr holding @c sqlite3*.
     *
     * @details Uses @c sqlite3_close_v2() rather than @c sqlite3_close() so that
     * the handle is deferred-closed if any @c sqlite3_stmt* derived from this
     * connection has not yet been finalized, preventing "database is locked"
     * errors during teardown.
     */
    struct Closer
    {
        /// @param[in] ptr  The @c sqlite3* handle to close. May be @c nullptr.
        void operator()(sqlite3* ptr) const noexcept
        {
            if (ptr != nullptr)
            {
                sqlite3_close_v2(ptr);
            }
        }
    };

    /// Owned database connection handle
    std::unique_ptr<sqlite3, Closer> _db;

    /// SQLite database file path
    std::filesystem::path _path;
};

/**
 * Create a temporary in-memory database.
 * @return A new @c SqliteDatabase instance connected to an in-memory database.
 * @note Each in-memory database is independent and ceases to exist as soon as
 *       the database connection is closed.
 */
export inline SqliteConnection InMemory() { return SqliteConnection{":memory:"}; }
} // namespace tewi::engine
