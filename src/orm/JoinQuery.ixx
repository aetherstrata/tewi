export module tewi:join;

import :constraint_helpers;
import :type_adapter;
import :query_state;

import std;

// ============================================================================
//  Join tag types (mirrors the API in the request)
// ============================================================================

export namespace tewi
{
/// Carries multiple row types for a multi-table select projection.
template <typename... Ts>
struct Select
{};

/// Marks the FROM table in a structured join expression.
template <typename T>
struct From
{};

/// Auto-FK join (requires ForeignKey<> on one side).
template <typename T>
struct Join
{};

/// Explicit join condition: JoinOn<&A::field, &B::field>
template <auto LeftMember, auto RightMember>
struct JoinOn
{};

// ============================================================================
// JoinQuery  - two-table INNER JOIN with explicit ON clause
// ============================================================================

/// Produces std::pair<LRow, RRow> results from an INNER JOIN.
template <typename LTable, typename RTable>
requires detail::IsTable<LTable> && detail::IsTable<RTable>
class JoinQuery
{
public:
    using left_row    = LTable::RowType;
    using right_row   = RTable::RowType;
    using result_type = std::pair<left_row, right_row>;

    JoinQuery(engine::SqliteConnection& db, detail::QueryState state, std::string_view left_col,
              std::string_view right_col)
        : _db(db)
        , _state(std::move(state))
        , _left_col(left_col)
        , _right_col(right_col)
    {}

    // ----------------------------------------------------------------
    //  Filtering / ordering on the join result
    // ----------------------------------------------------------------
    template <auto MP, typename V>
    [[nodiscard]] JoinQuery where(V&& v) &&
    {
        // MP must belong to LTable or RTable; we try both.
        constexpr std::string_view lc = LTable::template ColumnOf<MP>::ColumnName;
        constexpr std::string_view rc = RTable::template ColumnOf<MP>::ColumnName;

        static_assert(!lc.empty() || !rc.empty(), "Where<MP>: member not in joined tables.");

        std::string col;
        if constexpr (!lc.empty())
        {
            col = std::string(LTable::TableName) + "." + std::string(lc);
        }
        else if constexpr (!rc.empty())
        {
            col = std::string(RTable::TableName) + "." + std::string(rc);
        }

        return push_where(col, "=", std::forward<V>(v));
    }

    template <auto MP>
    [[nodiscard]] JoinQuery orderBy(Order order = Order::ASC) &&
    {
        constexpr std::string_view lc = LTable::template columnNameOf<MP>();
        constexpr std::string_view rc = RTable::template columnNameOf<MP>();

        static_assert(!lc.empty() || !rc.empty(), "OrderBy<MP>: member not in joined tables.");

        std::string col;
        if constexpr (!lc.empty())
        {
            col = std::string(LTable::TableName) + "." + std::string(lc);
        }
        else
        {
            col = std::string(RTable::TableName) + "." + std::string(rc);
        }

        _state.order_clauses.emplace_back(col, order);
        return std::move(*this);
    }

    [[nodiscard]] JoinQuery limit(i32 n) &&
    {
        auto query         = *this;
        query._state.limit = n;
        return query;
    }

    // ----------------------------------------------------------------
    //  Terminal
    // ----------------------------------------------------------------
    [[nodiscard]] std::vector<result_type> toVector()
    {
        const std::string sql = build_sql();
        auto stmt             = _db.prepare(sql);
        _state.bind_where(stmt);
        std::vector<result_type> out;
        while (stmt.step())
        {
            out.emplace_back(LTable::hydrate(stmt, 0),
                             RTable::hydrate(stmt, static_cast<i32>(LTable::ColumnsCount)));
        }
        return out;
    }

private:
    engine::SqliteConnection& _db;
    detail::QueryState _state;
    std::string_view _left_col;
    std::string_view _right_col;

    [[nodiscard]] std::string build_sql() const
    {
        return "SELECT " + LTable::column_list(LTable::TableName) + ", "
               + RTable::column_list(RTable::TableName) + " FROM "
               + std::string(LTable::TableName) + " INNER JOIN " + std::string(RTable::TableName)
               + " ON " + std::string(LTable::TableName) + "." + std::string(_left_col) + " = "
               + std::string(RTable::TableName) + "." + std::string(_right_col)
               + _state.where_sql() + _state.order_sql() + _state.limit_sql() + ";";
    }

    template <typename V>
    requires SqliteAdaptable<V>
    [[nodiscard]] JoinQuery push_where(std::string col, std::string op, V&& value)
    {
        using RV    = std::remove_cvref_t<V>;
        RV captured = std::forward<V>(value);

        detail::WherePredicate pred{
            .column = std::move(col),
            .op     = std::move(op),
            .binder = [captured](engine::SqliteStatement& stmt, i32 idx)
            {
                SqliteTypeAdapter<RV>::bind(stmt, idx, captured);
            }
        };

        _state.predicates.push_back(std::move(pred));
        return std::move(*this);
    }
};
} // namespace tewi