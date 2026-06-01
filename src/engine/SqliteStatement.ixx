module;
#include <sqlite3.h>
export module tewi:sqlite_statement;

import :sqlite_error;
import :number_types;

import std;

namespace tewi::engine
{
/**
 * @class SqliteStatement
 * @brief RAII owner of a compiled @c sqlite3_stmt* prepared statement.
 *
 * @details Compiles the SQL string on construction and calls
 * @c sqlite3_finalize() on destruction. Supports chainable @c bind() overloads
 * for all SQLite-native types, as well as row iteration via @c step() and
 * @c each().
 *
 * A @c SqliteStatement may be reset and re-bound for repeated execution without
 * re-compilation, which is significantly faster than calling @c Prepare() on
 * every iteration.
 *
 * @par Example: re-use with reset
 * @code{.cpp}
 *   auto stmt = db.Prepare("INSERT INTO log (msg) VALUES (?);");
 *   for (const auto& msg : messages) {
 *       stmt.bind(1, msg).exec();
 *       stmt.reset();
 *   }
 * @endcode
 */
export class SqliteStatement
{
public:
    /**
     * @brief Compiles a SQL statement against the given database connection.
     *
     * @param[in] db   Raw @c sqlite3* connection. Must remain valid for the
     *                 lifetime of this @c SqliteStatement.
     * @param[in] sql  SQL text to compile. Must be a single statement.
     *
     * @throws SqliteError if @c sqlite3_prepare_v2() fails.
     */
    SqliteStatement(sqlite3* db, std::string_view sql);

    /// @brief Deleted copy constructor - statement handles are not copyable.
    SqliteStatement(const SqliteStatement&) = delete;
    /// @brief Deleted copy-assignment - statement handles are not copyable.
    SqliteStatement& operator=(const SqliteStatement&) = delete;
    /// @brief Move constructor. Transfers ownership of the @c sqlite3_stmt* handle.
    SqliteStatement(SqliteStatement&&) = default;
    /// @brief Move-assignment operator.
    SqliteStatement& operator=(SqliteStatement&&) = default;

    /**
     * @brief Finalizes the compiled statement.
     *
     * @details When the @c SqliteStatement object goes out of scope or is explicitly
     * destroyed, the underlying @c std::unique_ptr<sqlite3_stmt,Finalizer> member
     * (@c _stmt) is automatically destroyed, invoking @c Finalizer::operator() which
     * calls @c sqlite3_finalize().
     *
     * @c sqlite3_finalize() is safe to call at any point in the statement's
     * lifecycle - whether it has never been stepped, is mid-iteration, or has
     * already run to @c SQLITE_DONE. Any pending result rows are simply
     * discarded.
     *
     * If the most recent @c Step() returned an error, @c sqlite3_finalize() will
     * return that same error code. This error is deliberately ignored here
     * because:
     * -# The error was already surfaced as a @c SqliteError exception by @c Step().
     * -# Throwing from a destructor during stack unwinding would invoke
     *    @c std::terminate().
     *
     * @note This destructor is @c noexcept by default because
     *       @c unique_ptr::~unique_ptr() is @c noexcept and @c sqlite3_finalize()
     *       is documented as always returning without side effects beyond
     *       freeing resources.
     *
     * @warning A @c SqliteStatement must not outlive the @c SqliteDatabase it was
     *          prepared against. Finalizing a statement after its parent connection
     *          has been closed is undefined behaviour at the SQLite C API level.
     */
    ~SqliteStatement() = default;

    /// @name Bind overloads
    /// @{

    /**
     * @brief Binds a 32-bit integer value to a positional parameter.
     *
     * @param[in] idx  1-based parameter index.
     * @param[in] val  Integer value to bind.
     * @return Reference to @c *this to allow method chaining.
     * @throws SqliteError on failure.
     */
    SqliteStatement& bind(i32 idx, i32 val);

    /**
     * @brief Binds a 64-bit integer value to a positional parameter.
     *
     * @param[in] idx  1-based parameter index.
     * @param[in] val  64-bit integer value to bind.
     * @return Reference to @c *this for chaining.
     * @throws SqliteError on failure.
     */
    SqliteStatement& bind(i32 idx, i64 val);

    /**
     * @brief Binds a @c double value to a positional parameter.
     *
     * @param[in] idx  1-based parameter index.
     * @param[in] val  Floating-point value to bind.
     * @return Reference to @c *this for chaining.
     * @throws SqliteError on failure.
     */
    SqliteStatement& bind(i32 idx, f64 val);

    /**
     * @brief Binds a text value to a positional parameter.
     *
     * @details Uses @c SQLITE_TRANSIENT so SQLite copies the string data
     * immediately. It is therefore safe to bind a temporary @c std::string
     * or any other short-lived @c std::string_view.
     *
     * @param[in] idx  1-based parameter index.
     * @param[in] val  String view whose contents are copied into the statement.
     * @return Reference to @c *this for chaining.
     * @throws SqliteError on failure.
     */
    SqliteStatement& bind(i32 idx, std::string_view val);

    /**
     * @brief Binds a blob value to a positional parameter.
     *
     * @details Uses @c SQLITE_TRANSIENT so SQLite copies the raw data
     * immediately. It is therefore safe to bind a temporary @c std::vector
     * or any other short-lived @c std::array.
     *
     * @param[in] idx  1-based parameter index.
     * @param[in] val  Span whose contents are copied into the statement.
     * @return Reference to @c *this for chaining.
     * @throws SqliteError on failure.
     */
    SqliteStatement& bind(i32 idx, std::span<const std::byte> val);

    /**
     * @brief Binds a SQL @c NULL to a positional parameter.
     *
     * @param[in] idx  1-based parameter index.
     * @param[in] _    @c nullptr sentinel (pass @c nullptr at the call site).
     * @return Reference to @c *this for chaining.
     * @throws SqliteError on failure.
     */
    SqliteStatement& bind(i32 idx, std::nullptr_t);

    /**
     * @brief Binds a value to a named parameter
     *
     * @details Looks up the parameter index via @c sqlite3_bind_parameter_index()
     * and delegates to the corresponding positional @c Bind() overload.
     *
     * @note The allowed syntax to declare a named parameter is one of the following:
     * (`:name`, `\@name` and `$name`)
     *
     * @param[in] name  Parameter name including the leading sigil, e.g. @c ":id".
     * @param[in] val   Value to bind; any type accepted by the positional overloads.
     * @tparam T        The type of the value to bind, deduced from the argument. Must be
     *                  accepted by one of the positional @c Bind() overloads.
     * @return Reference to @c *this for chaining.
     * @throws SqliteError if the name is not found or the underlying bind fails.
     */
    template <typename T>
    SqliteStatement& bind(std::string_view name, T&& val)
    {
        i32 idx = sqlite3_bind_parameter_index(_stmt.get(), std::string(name).c_str());
        if (idx == 0) throw SqliteError("Unknown bind parameter: " + std::string(name));
        return bind(idx, std::forward<T>(val));
    }

    /// @}
    /// @name Execution
    /// @{

    /**
     * @brief Advances the statement by one row
     *
     * @details Calls @c sqlite3_step() and interprets the result:
     * - @c SQLITE_ROW  → a row is ready; returns @c true.
     * - @c SQLITE_DONE → iteration finished; returns @c false.
     * - Anything else → throws @c SqliteError.
     *
     * @return @c true if a result row is available; @c false when the statement
     *         has been fully executed.
     *
     * @throws SqliteError on any SQLite error.
     */
    bool step();

    /**
     * @brief Executes the statement to completion and resets it.
     *
     * @details Drains all result rows (if any) and calls @c Reset(). Intended
     * for @c INSERT, @c UPDATE, and @c DELETE where row data is not needed.
     *
     * @throws SqliteError if @c Step() encounters an error.
     */
    void exec();

    /**
     * @brief Resets the statement so it can be re-executed with new bindings.
     *
     * @details Calls @c sqlite3_reset(). This call is cheap, it does not re-compile
     * the statement. Call @c ClearBinding() afterwards if the old bound values
     * should not be reused.
     */
    void reset() noexcept;

    /**
     * @brief Clears all parameter bindings, resetting them to @c NULL.
     *
     * @details Calls @c sqlite3_clear_bindings(). Typically paired with
     * @c Reset() before re-binding all parameters for a new execution.
     */
    void clearBindings() noexcept;

    /// @}
    /// @name Accessors
    /// @{

    /**
     * @brief Reads a result column as a 32-bit integer.
     * @param[in] col  0-based column index.
     * @return Integer value of the column.
     */
    [[nodiscard]] i32 columnInt(i32 col) const;

    /**
     * @brief Reads a result column as a 64-bit integer.
     * @param[in] col  0-based column index.
     * @return 64-bit integer value of the column.
     */
    [[nodiscard]] i64 columnLong(i32 col) const;

    /**
     * @brief Reads a result column as a @c double.
     * @param[in] col  0-based column index.
     * @return Floating-point value of the column.
     */
    [[nodiscard]] f64 columnDouble(i32 col) const;

    /**
     * @brief Reads a result column as a UTF-8 @c std::string.
     *
     * @details Returns an empty string if the column value is @c NULL.
     * Use @c ColumnIsNull() to distinguish @c NULL from an empty string.
     *
     * @param[in] col  0-based column index.
     * @return Text content of the column, or @c "" for @c NULL.
     */
    [[nodiscard]] std::string columnText(i32 col) const;

    /**
     * @brief Reads a result column as a binary blob.
     *
     * @details Calls @c sqlite3_column_blob() to obtain a pointer to the raw byte data
     * and @c sqlite3_column_bytes() to obtain its length.
     *
     * @warning The pointer returned by @c sqlite3_column_blob() is only valid until the
     * next call to a column accessor or @c Step() on this statement so retaining it
     * directly would be unsafe.
     *
     * @note The caller may use the returned @c std::span to access
     * the bytes without copying if they can guarantee that no other column accessors
     * or @c Step() calls will be made before the data is consumed.
     *
     * @param[in] col  0-based column index.
     *
     * @return A @c std::vector containing a copy of the column's raw bytes,
     * or an empty @c std::vector if the column value is @c SQL_NULL / zero-length values.
     */
    [[nodiscard]] std::span<const std::byte> columnBlob(i32 col) const;

    /**
     * @brief Checks whether a result column holds a SQL @c NULL value.
     * @param[in] col  0-based column index.
     * @return @c true if the column type is @c SQLITE_NULL.
     */
    [[nodiscard]] bool columnIsNull(i32 col) const;

    /**
     * @brief Returns the number of columns in the result set.
     * @return Column count, or 0 for statements that produce no rows.
     */
    [[nodiscard]] i32 columnCount() const;

    /**
     * @brief Returns the declared name of a result column.
     * @param[in] col  0-based column index.
     * @return Column name as declared in the SQL or assigned via @c AS.
     */
    [[nodiscard]] std::string columnName(i32 col) const;

    /**
     * @brief Iterates all result rows, invoking a callback for each one.
     *
     * @details Calls @c Step() until it returns @c false, invoking @p fn on each
     * row with a reference to @c *this so that column accessors can be called
     * inside the callback. Calls @c Reset() after the last row.
     *
     * @par Example
     * @code{.cpp}
     *   stmt.Each([](engine::SqliteStatement& s) {
     *       std::cout << s.ColumnInt(0) << " " << s.ColumnText(1) << "\n";
     *   });
     * @endcode
     *
     * @param[in] fun Callable with signature @c void(SqliteStatement&).
     *
     * @throws SqliteError if @c Step() encounters an error.
     */
    template <typename F>
    void each(F&& fun)
    {
        while (step())
        {
            std::forward<F>(fun)(*this);
        }
        reset();
    }

    /// @}

    /**
     * @brief Returns the raw @c sqlite3_stmt* handle for low-level API access.
     *
     * @warning The pointer is invalidated when this @c SqliteStatement is destroyed
     *          or move-assigned. Do not finalize it externally.
     *
     * @return Non-owning pointer to the compiled statement.
     */
    [[nodiscard]] sqlite3_stmt* handle() const noexcept;

private:
    /**
     * @brief Custom deleter that calls @c sqlite3_finalize() on the statement handle.
     */
    struct Finalizer
    {
        /// @param[in] ptr  SqliteStatement handle to finalize. May be @c nullptr.
        void operator()(sqlite3_stmt* ptr) const noexcept
        {
            if (ptr != nullptr) sqlite3_finalize(ptr);
        }
    };

    /// Owned compiled statement handle
    std::unique_ptr<sqlite3_stmt, Finalizer> _stmt;
};
} // namespace tewi
