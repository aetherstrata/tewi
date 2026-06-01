export module tewi:fk_helpers;

import :contraints;

import std;

namespace tewi::detail
{

// -----------------------------------------------------------------------
//  FK detection helpers
// -----------------------------------------------------------------------
template <typename FK>
struct is_foreign_key : std::false_type
{};

template <typename RT, auto RM>
struct is_foreign_key<ForeignKey<RT, RM>> : std::true_type
{};

template <typename FK>
constexpr bool isForeignKey = is_foreign_key<FK>::value;

template <typename C, typename TargetTable>
struct IsFkTo : std::false_type
{};

template <typename TargetTable, auto MP>
struct IsFkTo<ForeignKey<TargetTable, MP>, TargetTable> : std::true_type
{};

// Does a Column carry a ForeignKey pointing to TargetTable?
template <typename Col, typename TargetTable>
struct ColHasFkTo
{
private:
    template <typename... Cs>
    static consteval bool check(std::tuple<Cs...>*)
    {
        return (IsFkTo<Cs, TargetTable>::value || ...);
    }

public:
    static constexpr bool value = ColHasFkTo::check(static_cast<Col::constraint_pack*>(nullptr));
};

// -----------------------------------------------------------------------
//  find_fk_column_in<TableType, TargetTable>
//
//  Walks TableType's column tuple looking for a Column that has a
//  ForeignKey<TargetTable, …> constraint.  Returns the Column type,
//  or void if none is found.
// -----------------------------------------------------------------------
template <typename TableType, typename TargetTable, typename ColTuple>
struct FindFkColumn
{
    using type = void;
};

// Base case: empty tuple → not found
template <typename TableType, typename TargetTable>
struct FindFkColumn<TableType, TargetTable, std::tuple<>>
{
    using type = void;
};

// Recursive case: check head, recurse on tail
template <typename TableType, typename TargetTable, typename Head, typename... Tail>
struct FindFkColumn<TableType, TargetTable, std::tuple<Head, Tail...>>
{
private:
    // Does Head carry a ForeignKey pointing to TargetTable?
    static constexpr bool head_matches = []<typename... Cs>(std::tuple<Cs...>*)
    {
        return ([]<typename C>()
        {
            if constexpr (is_foreign_key<C>::value)
            {
                return std::is_same_v<typename C::Table, TargetTable>;
            }
            return false;
        }.template operator()<Cs>() || ...);
    }(static_cast<Head::Constraints*>(nullptr));

public:
    using type = std::conditional_t<
        head_matches, Head,
        typename FindFkColumn<TableType, TargetTable, std::tuple<Tail...>>::type>;
};

// Convenience alias
export template <typename TableType, typename TargetTable>
using FkColumn = FindFkColumn<TableType, TargetTable, typename TableType::ColumnsTuple>::type;

// Concept: TableType has a FK column pointing to TargetTable
export template <typename TableType, typename TargetTable>
concept HasFkTo = !std::is_void_v<FkColumn<TableType, TargetTable>>;

// -----------------------------------------------------------------------
//  Given a FK Column type, extract the referenced column name.
//  Walks the Column's constraints to find the ForeignKey<> tag.
// -----------------------------------------------------------------------
export template <typename FkCol, typename TargetTable>
consteval std::string_view fk_referenced_col_name()
{
    std::string_view result{};
    [&]<typename... Cs>(std::tuple<Cs...>*)
    {
        ([&]<typename C>()
        {
            if constexpr (IsFkTo<C, TargetTable>::value)
            {
                result = TargetTable::template ColumnOf<C::Member>::ColumnName;
            }
        }.template operator()<Cs>(), ...);
    }(static_cast<FkCol::Constraints*>(nullptr));
    return result;
}

// Called only when LT has a FK pointing to RT.
template <typename LT, typename RT>
requires HasFkTo<LT, RT>
consteval auto resolve_fk_forward()
{
    using FK = FkColumn<LT, RT>;
    return std::pair<std::string_view, std::string_view>{
        FK::ColumnName,
        fk_referenced_col_name<FK, RT>()
    };
}

// Called only when RT has a FK pointing to LT.
template <typename LT, typename RT>
requires HasFkTo<RT, LT>
consteval auto resolve_fk_reverse()
{
    using FK = FkColumn<RT, LT>;
    return std::pair<std::string_view, std::string_view>{
        fk_referenced_col_name<FK, LT>(),
        FK::ColumnName
    };
}

} // namespace tewi::detail
