module;
#include <sqlite3.h>
export module tewi:sqlite_error;

import std;

export namespace tewi::engine
{
/**
 * @brief Exception thrown when any SQLite operation fails.
 *
 * @details Inherits from @c std::runtime_error. The @c what() message includes
 * the caller-supplied context string, the integer SQLite result code, and the
 * human-readable description returned by @c sqlite3_errstr().
 */
struct SqliteError : std::runtime_error
{
    /**
     * @brief Construct a SqliteError with a message and SQLite result code.
     *
     * @param[in] msg     Human-readable description of the operation that failed.
     * @param[in] rc      SQLite result code (e.g. `SQLITE_CONSTRAINT`). Defaults to
     *                    `SQLITE_ERROR` when no specific code is available.
     */
    explicit SqliteError(std::string_view msg, int rc = SQLITE_ERROR) noexcept
        : std::runtime_error(std::format("{} {}", msg, PrettyCode(rc)))
        , _code(rc)
    {}

    /**
     * @brief Returns the raw SQLite result code associated with this error.
     * @return SQLite result code (one of the `SQLITE_*` integer constants).
     */
    [[nodiscard]] int code() const noexcept { return _code; }

    /**
     * @brief Get the string representation of an SQLite error code.
     * @param rc SQLite result code to format.
     * @return A string containing the SQLite error code and its text description.
     */
    [[nodiscard]] static std::string PrettyCode(int rc) noexcept
    {
        return std::format("[SQLite error {}: {}]", std::to_string(rc), sqlite3_errstr(rc));
    }

private:
    int _code; ///< SQLite result code stored for programmatic inspection.
};
} // namespace tewi::engine
