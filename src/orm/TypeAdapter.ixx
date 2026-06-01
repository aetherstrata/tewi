export module tewi:type_adapter;

import :sqlite_connection;
import :sqlite_statement;

import std;

export namespace tewi
{
/**
 * @brief Primary adapter template for mapping C++ types to SQLite values.
 *
 * This template defines the interface used to bind values of type `T` to a
 * SQLite statement and to read values of type `T` from a result column.
 * Specializations should provide the SQLite type affinity, nullability
 * information, and implementations for binding and reading.
 *
 * @tparam T C++ type adapted for SQLite storage and retrieval.
 */
template <typename T>
struct SqliteTypeAdapter
{
    /**
     * @brief Declares the SQLite column affinity associated with @c T
     *
     * @note Specializations should override this value with the appropriate
     *       SQLite affinity string, such as @c "INTEGER", @c "REAL", @c "TEXT",
     *       or @c "BLOB"
     */
    static constexpr std::string_view affinity = "UNKNOWN";

    /**
     * @brief Indicates whether values of @c T may be stored as SQL @c NULL
     *
     * @note Specializations should set this to @c true for nullable types
     *       such as @c std::optional<T>
     */
    static constexpr bool nullable = false;

    /**
     * @brief Binds a value to a SQLite statement parameter
     *
     * Writes the provided C++ value into the parameter slot identified by
     * @p idx in the given SQLite statement
     *
     * @param stmt Target SQLite statement
     * @param idx One-based parameter index
     * @param val Value to bind
     */
    static void bind(engine::SqliteStatement& stmt, i32 idx, const T& val);

    /**
     * @brief Reads a value from a SQLite result column.
     *
     * Extracts the value at the specified column index from the provided
     * SQLite statement and converts it to type @c T
     *
     * @param stmt Source SQLite statement
     * @param idx Zero-based column index
     * @return Value converted to type @c T
     */
    static T read(const engine::SqliteStatement& stmt, i32 idx);
};

/**
 * @brief Forwarding specialization for cv/ref-qualified types
 *
 * Reuses the adapter for the unqualified base type so that const-, volatile-,
 * and reference-qualified forms of a type do not require separate adapter
 * specializations.
 *
 * @tparam T Cv/ref-qualified type whose adapter should resolve to its
 * unqualified base type
 */
template <typename T>
requires (!std::same_as<T, std::remove_cvref_t<T>>)
struct SqliteTypeAdapter<T> : SqliteTypeAdapter<std::remove_cvref_t<T>>
{};

/**
 * @brief Checks whether a type provides a valid SQLite adapter
 *
 * A type satisfies this concept when its corresponding
 * @c SqliteTypeAdapter<std::remove_cvref_t<T>> exposes a compatible
 * @c bind() and @c read() interface for SQLite statement interaction.
 *
 * @tparam T Type to validate for SQLite adaptation support
 */
template <typename T>
concept SqliteAdaptable = requires(
    engine::SqliteStatement& stmt, const engine::SqliteStatement& cstmt,
    int idx, const std::remove_cvref_t<T>& val)
{
    { SqliteTypeAdapter<std::remove_cvref_t<T>>::bind(stmt, idx, val) } -> std::same_as<void>;
    { SqliteTypeAdapter<std::remove_cvref_t<T>>::read(cstmt, idx)     } -> std::same_as<std::remove_cvref_t<T>>;
};
} // namespace tewi