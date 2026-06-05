module tewi:fk_helpers;

import :contraints;
import :member_traits;

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
struct ColHasFkTo final
{
private:
    template <typename... Cs>
    static consteval bool check(std::tuple<Cs...>*)
    {
        return anyOf<IsFkTo<Cs, TargetTable>::value...>;
    }

public:
    static constexpr bool value = ColHasFkTo::check(static_cast<Col::Constraints*>(nullptr));
};

// -----------------------------------------------------------------------
//  find_fk_column_in<TableType, TargetTable>
//
//  Walks TableType's column tuple looking for a Column that has a
//  ForeignKey<TargetTable, …> constraint.  Returns the Column type,
//  or void if none is found.
// -----------------------------------------------------------------------
template <typename TargetTable, typename... Cols>
struct FindFkTo
{
    using type = void;
};

// Base case: empty tuple → not found
template <typename TargetTable>
struct FindFkTo<TargetTable>
{
    using type = void;
};

// Recursive case: check head, recurse on tail
template <typename TargetTable, typename First, typename... Rest>
struct FindFkTo<TargetTable, First, Rest...>
{
    using type = std::conditional_t<
        ColHasFkTo<First, TargetTable>::value, First,
        typename FindFkTo<TargetTable, Rest...>::type>;
};

// Concept: TableType has a FK column pointing to TargetTable
template <typename TableType, typename TargetTable>
concept HasFkTo = !std::is_void_v<typename TableType::template FkTo<TargetTable>>;

// -----------------------------------------------------------------------
//  Given a FK Column type, extract the referenced column name.
//  Walks the Column's constraints to find the ForeignKey<> tag.
// -----------------------------------------------------------------------
template <typename FkCol, typename TargetTable>
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
template <typename LT, typename RT>
requires HasFkTo<LT, RT>
consteval auto resolve_fk_forward()
{
    using FK = LT::template FkTo<RT>;
    return std::pair<std::string_view, std::string_view>{
        FK::columnName,
        fk_referenced_col_name<FK, RT>()
    };
}

// Called only when RT has a FK pointing to LT.
template <typename LT, typename RT>
requires HasFkTo<RT, LT>
consteval auto resolve_fk_reverse()
{
    using FK = RT::template FkTo<LT>;
    return std::pair<std::string_view, std::string_view>{
        fk_referenced_col_name<FK, LT>(),
        FK::columnName
    };
}

template <typename T, typename... Cs>
consteval bool fk_has_same_type()
{
    // No constraints, or no foreign keys at all: return true
    if constexpr (sizeof...(Cs) == 0 || !anyOf<isForeignKey<Cs>...>)
    {
        return true;
    }
    else
    {
        auto check = []<typename C>() consteval
        {
            if constexpr (isForeignKey<C>)
            {
                using MP = std::remove_cv_t<decltype(C::member)>;
                using Field = member_ptr<MP>::FieldType;
                return std::is_same_v<Field, T>;
            }
            else return false;
        };
        return anyOf<check.template operator()<Cs>()...>;
    }
}

template <typename T, typename... Cs>
concept ForeignKeyHasSameType = fk_has_same_type<T, Cs...>();
} // namespace tewi::detail
