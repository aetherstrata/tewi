export module tewi:constraint_helpers;

import :contraints;
import :member_traits;

import std;

namespace tewi::detail
{
// -----------------------------------------------------------------------
//  PrimaryKey detection helpers
// -----------------------------------------------------------------------
template <typename PK>
struct is_primary_key : std::false_type
{};

template <bool AI>
struct is_primary_key<PrimaryKey<AI>> : std::true_type
{};

template <typename PK>
constexpr bool isPrimaryKey = is_primary_key<PK>::value;

template <typename PK>
struct is_primary_key_autoincrement : std::false_type
{};

template <>
struct is_primary_key_autoincrement<PrimaryKey<true>> : std::true_type
{};

template <typename PK>
constexpr bool isAutoincrementPK = is_primary_key_autoincrement<PK>::value;

template <typename T, typename Tuple>
struct tuple_prepend;

template <typename T, typename... Ts>
struct tuple_prepend<T, std::tuple<Ts...>>
{
    using type = std::tuple<T, Ts...>;
};

template <typename... Cols>
struct primary_key_tuple
{
    using type = std::tuple<>;
};

template <typename First, typename... Rest>
struct primary_key_tuple<First, Rest...>
{
private:
    using tail_type = typename primary_key_tuple<Rest...>::type;

public:
    using type = std::conditional_t<First::IsPrimaryKey,
                                    typename tuple_prepend<typename First::FieldType, tail_type>::type,
                                    tail_type>;
};

template <typename... Cols>
using primary_key_tuple_t = typename primary_key_tuple<Cols...>::type;

// -----------------------------------------------------------------------
//  Build full DDL suffix for a pack of constraints (except FK REFERENCES).
// -----------------------------------------------------------------------
template <typename... Cs>
[[nodiscard]] std::string constraint_ddl(std::string_view col_name)
{
    std::string sql{};
    ([&]()
    {
        using C = Cs;
        if constexpr (requires { C::Suffix; })
        {
            sql += C::Suffix;
        }
        if constexpr (requires { C::Expression; })
        {
            sql += " CHECK(" + C::Expression + ")";
        }
        if constexpr (requires { C::Pattern; })
        {
            sql += " CHECK(regexp('" + C::Pattern + "', " + col_name + "))";
        }
    }(), ...);
    return sql;
}

// Requires the table to have one primary key
export template <typename TableType>
concept HasPrimaryKey = TableType::PrimaryKeyCount > 0;

template <typename First, typename... Rest>
consteval bool unique_column_names()
{
    if constexpr (sizeof...(Rest) == 0)
    {
        return true;
    }
    else
    {
        return ((First::ColumnName != Rest::ColumnName) && ...) && unique_column_names<Rest...>();
    }
}

template<typename First, typename... Rest>
consteval bool unique_member_ptrs()
{
    if constexpr (sizeof...(Rest) == 0)
    {
        return true;
    }
    else
    {
        return (([]<typename Other>()
        {
            if constexpr (ComparableMemberPtr<First, Other>)
            {
                return First::MemberPtr != Other::MemberPtr;
            }
            else
            {
                return true;
            }
        }.template operator()<Rest>()) && ...) && unique_member_ptrs<Rest...>();
    }
}

export template<typename... Cols>
concept UniqueColumnNames = unique_column_names<Cols...>();

export template<typename... Cols>
concept UniqueMemberPointers = unique_member_ptrs<Cols...>();

// ============================================================================
// IsTable concept
// ============================================================================
export template <typename T>
concept IsTable = requires {
    T::TableName;
    T::ColumnsCount;
    typename T::RowType;
    typename T::ColumnsTuple;
};
} // namespace tewi::detail
