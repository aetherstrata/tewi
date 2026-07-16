export module tewi:table;

import :ast_spec;
import :ast_renderer;
import :fixed_string;
import :column;
import :index;
import :column_helpers;
import :pk_helpers;

import std;

// ============================================================================
//  Table<Name, Row, Columns...>
// ============================================================================

namespace tewi
{

/// Maps a C++ struct T to an SQLite table.
export template <FixedString name, typename T, typename ColPack, typename IdxPack = Indexes<>>
struct Table;

template <FixedString name, typename T, IColumn... Cols, typename... Idxs>
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

    /// How many columns carry a ForeignKey to OherTable. More than one means
    /// FkTo/hasFkTo cannot pick a side; the caller must be explicit.
    template <typename OherTable>
    static constexpr usize fkCountTo = detail::count_fks_to<OherTable, Cols...>;

    static constexpr std::string_view tableName = name_storage.view();

    static constexpr usize columnsCount    = sizeof...(Cols);
    static constexpr usize indexCount      = sizeof...(Idxs);
    static constexpr usize primaryKeyCount = ((Cols::isPrimaryKey ? usize{1} : usize{0}) + ...);

    static constexpr bool hasAutoIncrementPk = detail::anyOf<Cols::isAutoincrement...>;

    static_assert(primaryKeyCount > 0, "A table must have at least one PrimaryKey.");

    static_assert(!(hasAutoIncrementPk && primaryKeyCount != 1),
                  "AUTOINCREMENT primary keys are only allowed when the table has exactly one primary key column.");


    // AST construction. `constexpr` (not `consteval`) - it only needs to run
    // as part of a `consteval` caller's single evaluation, not force one on
    // its own; see Column::column_def.
    [[nodiscard]] static constexpr ast::TableDefNode creation_spec(bool ifNotExists)
    {
        ast::TableDefNode spec;
        spec.table                     = tableName;
        spec.if_not_exists             = ifNotExists;

        ([&]() constexpr
        {
            spec.columns.emplace_back(Cols::column_def());
            if constexpr (Cols::isPrimaryKey)
            {
                spec.constraints.emplace_back(ast::PrimaryKeyConstraintNode{
                    .column_name = std::string(Cols::columnName),
                    .autoincrement = Cols::isAutoincrement,
                });
            }
        }(), ...);

        (spec.indices.emplace_back(Idxs::template creation_spec<Table>(ifNotExists)), ...);

        return spec;
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
    //  Fully-qualified column name ("tableName.columnName") materialised as
    //  a FixedString with static storage duration.
    //  See BasicQuery's qualified_column_name for the multi-table variant.
    // -----------------------------------------------------------------------
    template <IColumn Col>
    static constexpr auto fully_qualified_name()
    {
        constexpr std::string_view tbl = tableName;
        constexpr std::string_view col = Col::columnName;
        constexpr std::size_t N        = tbl.size() + 1 + col.size() + 1;

        char buffer[N]{};
        std::size_t idx = 0;
        for (char c : tbl) { buffer[idx++] = c; }
        buffer[idx++] = '.';
        for (char c : col) { buffer[idx++] = c; }
        buffer[idx] = '\0';
        return FixedString<N>(buffer);
    }
};
namespace detail
{
template <typename T>
struct is_table : std::false_type
{};

template <FixedString name, typename T, typename ColPack, typename IdxPack>
struct is_table<Table<name, T, ColPack, IdxPack>> : std::true_type
{};
} // namespace detail

export template <typename T>
concept ITable = detail::is_table<T>::value;
} // namespace tewi