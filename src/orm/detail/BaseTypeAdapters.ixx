export module tewi:base_adapters;

import :sqlite_statement;
import :type_adapter;

import std;

namespace tewi
{
template <>
struct SqliteTypeAdapter<i32>
{
    static constexpr std::string_view affinity = "INTEGER";
    static constexpr bool nullable = false;

    static void bind(engine::SqliteStatement& stmt, const i32 idx, const i32& val)
    {
        stmt.bind(idx, val);
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

    static void bind(engine::SqliteStatement& stmt, const i32 idx, const i64& val)
    {
        stmt.bind(idx, val);
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

    static void bind(engine::SqliteStatement& stmt, const i32 idx, const f64& val)
    {
        stmt.bind(idx, val);
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

    static void bind(engine::SqliteStatement& stmt, const i32 idx, const f32& val)
    {
        stmt.bind(idx, static_cast<f64>(val));
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

    static void bind(engine::SqliteStatement& stmt, const i32 idx, const std::string& val)
    {
        stmt.bind(idx, val);
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

    static void bind(engine::SqliteStatement& stmt, const i32 idx, const bool& val)
    {
        stmt.bind(idx, val ? 1 : 0);
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

    static void bind(engine::SqliteStatement& stmt, const i32 idx, const std::vector<u8>& val)
    {
        stmt.bind(idx, std::as_bytes(std::span(val)));
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

    static void bind(engine::SqliteStatement& stmt, const i32 idx, const std::vector<std::byte>& val)
    {
        stmt.bind(idx, val);
    }
    static std::vector<std::byte> read(const engine::SqliteStatement& stmt, const i32 idx)
    {
        auto span = stmt.columnBlob(idx);
        return {span.data(), span.data() + span.size_bytes()};
    }
};

template <typename T>
requires SqliteAdaptable<T>
struct SqliteTypeAdapter<std::optional<T>>
{
    static constexpr std::string_view affinity = SqliteTypeAdapter<T>::affinity;
    static constexpr bool nullable = true;

    static void bind(engine::SqliteStatement& stmt, i32 idx, const std::optional<T>& val)
    {
        if (val) SqliteTypeAdapter<T>::bind(stmt, idx, *val);
        else     stmt.bind(idx, nullptr);
    }

    static std::optional<T> read(const engine::SqliteStatement& stmt, i32 idx)
    {
        if (stmt.columnIsNull(idx)) return std::nullopt;
        return SqliteTypeAdapter<T>::read(stmt, idx);
    }
};
} // namespace tewi
