export module tewi:table;

import :column;
import :index;
import :table_helpers;
import :pk_helpers;
import :type_adapter;

import std;

// ============================================================================
//  Table<Name, Row, Columns...>
// ============================================================================

namespace tewi
{

/// Maps a C++ struct T to a SQLite table.
export template <FixedString name, typename T, typename ColPack, typename IdxPack = Indexes<>>
struct Table;

template <FixedString name, typename T, typename... Cols, typename... Idxs>
requires detail::UniqueColumnNames<Cols...> && detail::UniqueMemberPointers<Cols...>
struct Table<name, T, Columns<Cols...>, Indexes<Idxs...>>
{
private:
    static constexpr auto name_storage = name;
public:
    using RowType = T;
    using KeyType = detail::PrimaryKeyType<Cols...>;

    using ColumnsTuple = std::tuple<Cols...>;
    using IndicesTuple = std::tuple<Idxs...>;
    using KeyTuple     = detail::PrimaryKeyColumns<Cols...>;

    template <auto MP>
    using ColumnOf = detail::mp_column<MP, Cols...>::type;

    template <typename OherTable>
    using FkTo = detail::FindFkTo<OherTable, Cols...>::type;

    template <typename OherTable>
    static constexpr bool hasFkTo = !std::is_void<FkTo<OherTable>>::value;

    static constexpr std::string_view tableName = name_storage.view();

    static constexpr usize columnsCount    = sizeof...(Cols);
    static constexpr usize indexCount      = sizeof...(Idxs);
    static constexpr usize primaryKeyCount = ((Cols::isPrimaryKey ? usize{1} : usize{0}) + ...);

    static constexpr bool hasAutoIncrementPk = detail::anyOf<Cols::isAutoincrement...>;

    static_assert(primaryKeyCount > 0, "A table must have at least one PrimaryKey.");

    static_assert(!(hasAutoIncrementPk && primaryKeyCount != 1),
                  "AUTOINCREMENT primary keys are only allowed when the table has exactly one primary key column.");

    // -----------------------------------------------------------------------
    //  DDL
    // -----------------------------------------------------------------------
    [[nodiscard]] static std::string create_table_sql(bool ifNotExists = true)
    {
        std::string sql = "CREATE TABLE ";
        if (ifNotExists) sql += "IF NOT EXISTS ";
        sql        += std::string(tableName) + " (";
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
            sql   += Cols::columnName;
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
            auto& fld = obj.*(Cols::member);
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
            if constexpr (Cols::isPrimaryKey)
            {
                if (!first) sql += " AND ";
                sql += Cols::columnName;
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
            if constexpr (Cols::isPrimaryKey)
            {
                using FT = Cols::FieldType;
                const auto& fld = obj.*(Cols::member);
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
            const auto& fld = obj.*(Cols::member);
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
            if constexpr (Cols::isPrimaryKey)
            {
                if (!first) sql += ", ";
                sql += Cols::columnName;
                if constexpr (Cols::isAutoincrement)
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

namespace detail
{
template <typename T>
struct is_table : std::false_type {};

template <FixedString name, typename T, typename ColPack, typename IdxPack>
struct is_table<Table<name, T, ColPack, IdxPack>> : std::true_type {};
} // namespace detail

export template <typename T>
concept IsTable = detail::is_table<T>::value;

} // namespace tewi