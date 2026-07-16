export module tewi:index;

import :ast_spec;
import :ast_renderer;
import :fixed_string;

import std;

// ============================================================================
//  Table Indices
// ============================================================================

namespace tewi
{

// Plain index
export template <FixedString name, auto... memberPtrs>
struct Index
{
    static constexpr auto name_storage          = name;
    static constexpr std::string_view IndexName = name_storage.view();

    static constexpr bool unique = false;

    // AST construction. `constexpr` (not `consteval`) - see Column::column_def
    // for why: it only needs to run as part of a `consteval` caller's single
    // evaluation, not force one on its own.
    template <typename TableType>
    [[nodiscard]] static constexpr ast::IndexDefNode creation_spec(bool ifNotExists)
    {
        ast::IndexDefNode spec;
        spec.index_name    = IndexName;
        spec.table         = TableType::tableName;
        spec.if_not_exists = ifNotExists;
        spec.unique        = unique;

        // Fold over MemberPtrs, resolving each to its column name at compile time.
        ([&]<auto MP>()
        {
            constexpr std::string_view col = TableType::template ColumnOf<MP>::columnName;
            static_assert(!col.empty(),
                          "Index: member pointer not mapped to any column in this table.");
            spec.columns.emplace_back(col);
        }.template operator()<memberPtrs>(), ...);

        return spec;
    }
};

// Unique index
export template <FixedString IndexName, auto... MemberPtrs>
struct UniqueIndex : Index<IndexName, MemberPtrs...>
{
    static constexpr bool unique = true; // shadows base
};

// Tag to delimit columns from indexes in the Table parameter list.
export template <typename... Is>
struct Indexes
{};

} // namespace tewi
