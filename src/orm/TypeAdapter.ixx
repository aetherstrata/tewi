module tewi:type_adapter;

import :sqlite_connection;
import :sqlite_statement;

import std;

namespace tewi
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
     * @brief Binds a value to a named SQLite statement parameter
     *
     * Writes the provided C++ value into the named parameter identified by
     * @p name in the given SQLite statement
     *
     * @param stmt Target SQLite statement
     * @param name Bind parameter name, including its sigil (e.g. ":p1")
     * @param val Value to bind
     */
    static void bind(engine::SqliteStatement& stmt, std::string_view name, const T& val);

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
    std::string_view name, int idx, const std::remove_cvref_t<T>& val)
{
    { SqliteTypeAdapter<std::remove_cvref_t<T>>::bind(stmt, name, val) } -> std::same_as<void>;
    { SqliteTypeAdapter<std::remove_cvref_t<T>>::read(cstmt, idx)      } -> std::same_as<std::remove_cvref_t<T>>;
};

template <>
struct SqliteTypeAdapter<i32>
{
    static constexpr std::string_view affinity = "INTEGER";
    static constexpr bool nullable = false;

    static void bind(engine::SqliteStatement& stmt, const std::string_view name, const i32& val)
    {
        stmt.bind(name, val);
    }
    static i32 read(const engine::SqliteStatement& stmt, const i32 idx)
    {
        return stmt.columnInt(idx);
    }
};

template <>
struct SqliteTypeAdapter<i64>
{
    static constexpr std::string_view affinity = "INTEGER";
    static constexpr bool nullable = false;

    static void bind(engine::SqliteStatement& stmt, const std::string_view name, const i64& val)
    {
        stmt.bind(name, val);
    }
    static i64 read(const engine::SqliteStatement& stmt, const i32 idx)
    {
        return stmt.columnLong(idx);
    }
};

template <>
struct SqliteTypeAdapter<f64>
{
    static constexpr std::string_view affinity = "REAL";
    static constexpr bool nullable = false;

    static void bind(engine::SqliteStatement& stmt, const std::string_view name, const f64& val)
    {
        stmt.bind(name, val);
    }
    static f64 read(const engine::SqliteStatement& stmt, const i32 idx)
    {
        return stmt.columnDouble(idx);
    }
};

template <>
struct SqliteTypeAdapter<f32>
{
    static constexpr std::string_view affinity = "REAL";
    static constexpr bool nullable = false;

    static void bind(engine::SqliteStatement& stmt, const std::string_view name, const f32& val)
    {
        stmt.bind(name, static_cast<f64>(val));
    }
    static f32 read(const engine::SqliteStatement& stmt, const i32 idx)
    {
        return static_cast<f32>(stmt.columnDouble(idx));
    }
};

template <>
struct SqliteTypeAdapter<std::string>
{
    static constexpr std::string_view affinity = "TEXT";
    static constexpr bool nullable = false;

    static void bind(engine::SqliteStatement& stmt, const std::string_view name, const std::string& val)
    {
        stmt.bind(name, val);
    }
    static std::string read(const engine::SqliteStatement& stmt, const i32 idx)
    {
        return stmt.columnText(idx);
    }
};

template <>
struct SqliteTypeAdapter<bool>
{
    static constexpr std::string_view affinity = "INTEGER";
    static constexpr bool nullable = false;

    static void bind(engine::SqliteStatement& stmt, const std::string_view name, const bool& val)
    {
        stmt.bind(name, val ? 1 : 0);
    }
    static bool read(const engine::SqliteStatement& stmt, const i32 idx)
    {
        return stmt.columnInt(idx) != 0;
    }
};

template <>
struct SqliteTypeAdapter<std::vector<u8>>
{
    static constexpr std::string_view affinity = "BLOB";
    static constexpr bool nullable = false;

    static void bind(engine::SqliteStatement& stmt, const std::string_view name, const std::vector<u8>& val)
    {
        stmt.bind(name, std::as_bytes(std::span(val)));
    }
    static std::vector<u8> read(const engine::SqliteStatement& stmt, const i32 idx)
    {
        auto span = stmt.columnBlob(idx);
        const auto *data_ptr = reinterpret_cast<const u8*>(span.data());
        return {data_ptr, data_ptr + span.size_bytes()};
    }
};

template <>
struct SqliteTypeAdapter<std::vector<std::byte>>
{
    static constexpr std::string_view affinity = "BLOB";
    static constexpr bool nullable = false;

    static void bind(engine::SqliteStatement& stmt, const std::string_view name, const std::vector<std::byte>& val)
    {
        stmt.bind(name, val);
    }
    static std::vector<std::byte> read(const engine::SqliteStatement& stmt, const i32 idx)
    {
        auto span = stmt.columnBlob(idx);
        return {span.data(), span.data() + span.size_bytes()};
    }
};

template <SqliteAdaptable T>
struct SqliteTypeAdapter<std::optional<T>>
{
    static constexpr std::string_view affinity = SqliteTypeAdapter<T>::affinity;
    static constexpr bool nullable = true;

    static void bind(engine::SqliteStatement& stmt, std::string_view name, const std::optional<T>& val)
    {
        if (val) SqliteTypeAdapter<T>::bind(stmt, name, *val);
        else     stmt.bind(name, nullptr);
    }

    static std::optional<T> read(const engine::SqliteStatement& stmt, i32 idx)
    {
        if (stmt.columnIsNull(idx)) return std::nullopt;
        return SqliteTypeAdapter<T>::read(stmt, idx);
    }
};
} // namespace tewi