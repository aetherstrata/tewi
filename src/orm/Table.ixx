export module tewi:table;

import :column;
import :constraint_helpers;
import :index;
import :type_adapter;

import std;

// ============================================================================
//  Table<Name, Row, Columns...>
// ============================================================================

export namespace tewi
{

/// Maps a C++ struct T to a SQLite table.
template <FixedString Name, typename T, typename ColPack, typename IdxPack = Indexes<>>
struct Table;

template <FixedString Name, typename T, typename... Cols, typename... Idxs>
requires detail::UniqueColumnNames<Cols...> && detail::UniqueMemberPointers<Cols...>
struct Table<Name, T, Columns<Cols...>, Indexes<Idxs...>>
{
    using RowType      = T;
    using ColumnsTuple = std::tuple<Cols...>;

    static constexpr auto name_storage = Name;
    static constexpr std::string_view TableName = name_storage.view();

    static constexpr usize ColumnsCount = sizeof...(Cols);
    static constexpr usize IndexCount   = sizeof...(Idxs);

    // -----------------------------------------------------------------------
    //  Compile-time column-name lookup by member pointer
    // -----------------------------------------------------------------------
    template <auto MP>
    [[nodiscard]] static consteval std::string_view column_name_for()
    {
        std::string_view result{};
        ([&]<typename Col>() constexpr
        {
            if constexpr (requires { Col::MemberPtr == MP; })
            {
                if (Col::MemberPtr == MP && result.empty())
                {
                    result = Col::ColumnName;
                }
            }
        }.template operator()<Cols>(), ...);
        return result;
    }

    // -----------------------------------------------------------------------
    //  Primary-key column name (first column tagged PrimaryKey<>)
    // -----------------------------------------------------------------------
    static consteval std::string_view find_pk()
    {
        std::string_view result{};
        ([&]() constexpr
        {
            if (Cols::IsPrimaryKey && result.empty())
            {
                result = Cols::ColumnName;
            }
        }(), ...);
        return result;
    }

    static constexpr std::string_view pk_name = find_pk();

    // -----------------------------------------------------------------------
    //  DDL
    // -----------------------------------------------------------------------
    [[nodiscard]] static std::string create_table_sql(bool ifNotExists = true)
    {
        std::string sql = "CREATE TABLE ";
        if (ifNotExists) sql += "IF NOT EXISTS ";
        sql        += std::string(TableName) + " (";
        bool first  = true;
        ([&]() constexpr
        {
            if (!first) sql += ", ";
            sql   += Cols::ddl();
            first  = false;
        }(), ...);
        sql += ");";
        return sql;
    }

    // emit all CREATE INDEX statements for this table.
    [[nodiscard]] static std::string create_indexes_sql(bool ifNotExists = true)
    {
        std::string sql;
        ([&]<typename Idx>()
        {
            sql += Idx::template create_index_sql<Table>(ifNotExists) + "\n";
        }.template operator()<Idxs>(), ...);
        return sql;
    }

    // -----------------------------------------------------------------------
    //  Column list for SELECT (with optional table prefix)
    // -----------------------------------------------------------------------
    [[nodiscard]] static std::string column_list(std::string_view prefix = {})
    {
        std::string sql;
        bool first = true;
        ([&]()
        {
            if (!first) sql += ", ";
            if (!prefix.empty())
            {
                sql += prefix;
                sql += ".";
            }
            sql   += Cols::ColumnName;
            first  = false;
        }(), ...);
        return sql;
    }

    // -----------------------------------------------------------------------
    //  Hydration: populate a T from a statement row, starting at col_offset
    // -----------------------------------------------------------------------
    [[nodiscard]] static T hydrate(const engine::SqliteStatement& stmt, const i32 colOffset = 0)
    {
        T obj{};
        i32 idx = colOffset;
        ([&]()
        {
            using FT  = Cols::FieldType;
            auto& fld = obj.*(Cols::MemberPtr);
            fld = SqliteTypeAdapter<FT>::read(stmt, idx++);
        }(), ...);
        return obj;
    }

    // -----------------------------------------------------------------------
    //  Binding: bind all fields of a T to a statement (bind_offset is 1-based)
    //  Returns the next available bind index.
    // -----------------------------------------------------------------------
    static i32 bind_all(engine::SqliteStatement& stmt, const T& obj, const i32 bindOffset = 1)
    {
        i32 idx = bindOffset;
        ([&]()
        {
            using FT  = Cols::FieldType;
            const auto& fld = obj.*(Cols::MemberPtr);
            SqliteTypeAdapter<FT>::bind(stmt, idx++, fld);
        }(), ...);
        return idx;
    }
};

} // namespace tewi