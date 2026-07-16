module tewi:table_helpers;

import :table;

namespace tewi
{

// Requires the table to have one primary key
template <typename TableType>
concept HasPrimaryKey = ITable<TableType> && TableType::primaryKeyCount > 0;

// Concept: TableType has a FK column pointing to TargetTable
template <typename TableType, typename TargetTable>
concept HasFkTo = ITable<TableType> && ITable<TargetTable>
                  && !std::is_void_v<typename TableType::template FkTo<TargetTable>>;

namespace detail
{
// -----------------------------------------------------------------------
//  Given a FK Column type, extract the referenced column name.
//  Walks the Column's constraints to find the ForeignKey<> tag.
// -----------------------------------------------------------------------
template <IColumn FkCol, ITable TargetTable>
consteval std::string_view fk_referenced_col_name()
{
    std::string_view result{};
    [&]<typename... Cs>(std::tuple<Cs...>*)
    {
        ([&]<typename C>()
        {
            if constexpr (IsFkTo<C, TargetTable>::value)
            {
                result = TargetTable::template ColumnOf<C::member>::columnName;
            }
        }.template operator()<Cs>(), ...);
    }(static_cast<FkCol::Constraints*>(nullptr));
    return result;
}

// Called only when LT has a FK pointing to RT.
template <ITable LT, ITable RT>
requires HasFkTo<LT, RT>
consteval std::pair<std::string_view, std::string_view> resolve_fk_forward()
{
    using FK = LT::template FkTo<RT>;
    return {
        FK::columnName,
        fk_referenced_col_name<FK, RT>()};
}

// Called only when RT has a FK pointing to LT.
template <ITable LT, ITable RT>
requires HasFkTo<RT, LT>
consteval std::pair<std::string_view, std::string_view> resolve_fk_reverse()
{
    using FK = RT::template FkTo<LT>;
    return {
        fk_referenced_col_name<FK, LT>(),
        FK::columnName};
}
} // namespace detail
} // namespace tewi
