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
private:
    static constexpr auto name_storage = Name;
public:
    using RowType      = T;
    using ColumnsTuple = std::tuple<Cols...>;
    using IndicesTuple = std::tuple<Idxs...>;
    using KeyTuple     = detail::PrimaryKeyColumns<Cols...>;
    using KeyType      = detail::PrimaryKeyType<Cols...>;

    template <auto MP>
    using ColumnOf = detail::mp_column<MP, Cols...>::type;

    static constexpr std::string_view TableName = name_storage.view();

    static constexpr usize ColumnsCount    = sizeof...(Cols);
    static constexpr usize IndexCount      = sizeof...(Idxs);
    static constexpr usize PrimaryKeyCount = ((Cols::IsPrimaryKey ? usize{1} : usize{0}) + ...);

    static constexpr bool HasAutoIncrementPrimaryKey = detail::anyOf<Cols::IsAutoincrement...>;

    static_assert(PrimaryKeyCount > 0, "A table must have at least one PrimaryKey.");

    static_assert(!(HasAutoIncrementPrimaryKey && PrimaryKeyCount != 1),
                  "AUTOINCREMENT primary keys are only allowed when the table has exactly one primary key column.");

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
        sql += ",";
        sql += primary_key_sql();
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

    [[nodiscard]] static std::string primary_key_where_clause()
    {
        std::string sql;
        bool first = true;
        ([&]()
        {
            if constexpr (Cols::IsPrimaryKey)
            {
                if (!first) sql += " AND ";
                sql += Cols::ColumnName;
                sql += " = ?";
                first = false;
            }
        }(), ...);
        return sql;
    }

    static i32 bind_primary_key(engine::SqliteStatement& stmt, const T& obj, const i32 bindOffset = 1)
    {
        i32 idx = bindOffset;
        ([&]()
        {
            if constexpr (Cols::IsPrimaryKey)
            {
                using FT = Cols::FieldType;
                const auto& fld = obj.*(Cols::MemberPtr);
                SqliteTypeAdapter<FT>::bind(stmt, idx++, fld);
            }
        }(), ...);
        return idx;
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

private:

    [[nodiscard]] static std::string primary_key_sql()
    {
        std::string sql = " PRIMARY KEY (";
        bool first      = true;
        ([&]() constexpr
        {
            if constexpr (Cols::IsPrimaryKey)
            {
                if (!first) sql += ", ";
                sql += Cols::ColumnName;
                if constexpr (Cols::IsAutoincrement)
                {
                    sql += " AUTOINCREMENT";
                }
                first = false;
            }
        }(), ...);
        sql += ")";
        return sql;
    }
};

} // namespace tewi