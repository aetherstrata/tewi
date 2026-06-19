export module tewi:order;

import std;

namespace tewi
{
/**
 * @brief Represents the SQL sorting direction for query results.
 *
 * Used in @c orderBy clauses to dictate how the database should sort the returned rows.
 */
export enum class Order
{
    ASC, /// Ascending order
    DESC /// Descending order
};

/**
 * @brief Converts an @c Order enumeration value to its corresponding SQL string representation.
 *
 * @param order The sorting order to convert.
 * @return A compile-time string view containing either "ASC" or "DESC".
 */
constexpr std::string_view toSql(Order order)
{
    switch (order)
    {
        case Order::ASC: return "ASC";
        case Order::DESC: return "DESC";
    }
    std::unreachable();
}
} // namespace tewi
