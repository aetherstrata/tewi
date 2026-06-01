export module tewi:query_state;

import :sqlite_statement;

import std;

// ============================================================================
//  QueryState  - WHERE / ORDER BY / LIMIT accumulated state
// ============================================================================

namespace tewi
{
export enum class Order { ASC, DESC };

namespace detail
{
struct WherePredicate
{
    std::string column;
    std::string op;
    std::move_only_function<void(engine::SqliteStatement&, i32) const> binder;
};

struct QueryState
{
    std::vector<WherePredicate> predicates;
    std::vector<std::pair<std::string, Order>> order_clauses;
    std::optional<i32> limit;
    std::optional<i32> offset_val;
    std::optional<std::string> distinct_col;
    bool distinct = false; ///< Affects the whole row

    [[nodiscard]] std::string where_sql() const
    {
        if (predicates.empty()) return {};
        std::string sql = " WHERE ";
        bool first      = true;
        for (const auto& pred : predicates)
        {
            if (!first) sql += " AND ";
            sql   += pred.column + " " + pred.op + " ?";
            first  = false;
        }
        return sql;
    }

    [[nodiscard]] std::string order_sql() const
    {
        if (order_clauses.empty()) return {};
        std::string sql = " ORDER BY ";
        bool first      = true;
        for (const auto& [col, ord] : order_clauses)
        {
            if (!first) sql += ", ";
            sql   += col + (ord == Order::DESC ? " DESC" : " ASC");
            first  = false;
        }
        return sql;
    }

    [[nodiscard]] std::string limit_sql() const
    {
        std::string sql;
        if (limit)
        {
            sql += " LIMIT " + std::to_string(*limit);
        }
        if (offset_val)
        {
            sql += " OFFSET " + std::to_string(*offset_val);
        }
        return sql;
    }

    void bind_where(engine::SqliteStatement& stmt) const
    {
        i32 idx = 1;
        for (const auto& pred : predicates)
        {
            pred.binder(stmt, idx++);
        }
    }
};
} // namespace detail
} // namespace tewi