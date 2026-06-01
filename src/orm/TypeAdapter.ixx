export module tewi:type_adapter;

import :sqlite_connection;
import :sqlite_statement;

import std;

export namespace tewi
{
template <typename T>
struct SqliteTypeAdapter
{
    /// @note Override with the desired column type
    static constexpr std::string_view affinity = "UNKNOWN";

    /// @note Override to true for @c std::optional<T>
    static constexpr bool nullable = false;

    /// Write: T -> SQLite binding slot
    static void bind(engine::SqliteStatement& stmt, i32 idx, const T& val);

    /// Read: SQLite column -> T
    static T read(const engine::SqliteStatement& stmt, i32 idx);
};

template <typename T>
requires (!std::same_as<T, std::remove_cvref_t<T>>)
struct SqliteTypeAdapter<T> : SqliteTypeAdapter<std::remove_cvref_t<T>>
{};

template <typename T>
concept SqliteAdaptable = requires(
    engine::SqliteStatement& stmt, const engine::SqliteStatement& cstmt,
    int idx, const std::remove_cvref_t<T>& val)
{
    { SqliteTypeAdapter<std::remove_cvref_t<T>>::bind(stmt, idx, val) } -> std::same_as<void>;
    { SqliteTypeAdapter<std::remove_cvref_t<T>>::read(cstmt, idx)     } -> std::same_as<std::remove_cvref_t<T>>;
};
} // namespace tewi