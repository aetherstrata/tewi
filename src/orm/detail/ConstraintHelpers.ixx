export module tewi:constraint_helpers;

import :contraints;
import :member_traits;

import std;

namespace tewi::detail
{
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

// -----------------------------------------------------------------------
//  PrimaryKey detection (handles both PK<true> and PK<false>)
// -----------------------------------------------------------------------
template <typename C>
struct is_primary_key : std::false_type
{};

template <bool AI>
struct is_primary_key<PrimaryKey<AI>> : std::true_type
{};

// Requires the table to have one primary key
export template <typename TableType>
concept HasPrimaryKey = !TableType::pk_name.empty();

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
