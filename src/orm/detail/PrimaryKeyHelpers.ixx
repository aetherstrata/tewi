export module tewi:pk_helpers;

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
    using tail_type = primary_key_tuple<Rest...>::type;

public:
    using type = std::conditional_t<First::IsPrimaryKey,
                                    typename tuple_prepend<First, tail_type>::type,
                                    tail_type>;
};

template <typename... Cols>
using PrimaryKeyColumns = primary_key_tuple<Cols...>::type;

template <typename... Cols>
struct primary_key_type
{
    using type = std::tuple<>;
};

template <typename First, typename... Rest>
struct primary_key_type<First, Rest...>
{
private:
    using tail_type = primary_key_type<Rest...>::type;

public:
    using type = std::conditional_t<First::IsPrimaryKey,
                                    typename tuple_prepend<typename First::FieldType, tail_type>::type,
                                    tail_type>;
};

template <typename... Cols>
using PrimaryKeyType = primary_key_type<Cols...>::type;

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
} // namespace tewi::detail
