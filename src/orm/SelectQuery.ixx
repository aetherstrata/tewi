export module tewi:select;

import :fk_helpers;
import :query_range;
import :sqlite_connection;
import :type_adapter;
import :join;

import std;

namespace tewi
{
// ============================================================================
//  §12  SelectQuery  - fluent LINQ-like builder
// ============================================================================

/// Fluent query builder.  All methods take *this by value and return a new
/// builder, so queries can be composed without mutation.
export template <typename TableType>
requires IsTable<TableType>
class SelectQuery
{

public:
    using RowType = TableType::RowType;

    explicit SelectQuery(engine::SqliteConnection& db)
        : _db(db)
    {}

    // ----------------------------------------------------------------
    //  WHERE - compile-time member pointer (preferred, zero runtime cost)
    // ----------------------------------------------------------------
    /// WHERE column = value  (member pointer as NTTP).
    template <auto MemberPtr, typename V>
    [[nodiscard]] SelectQuery where(V&& value) &&
    {
        constexpr std::string_view col = TableType::template ColumnOf<MemberPtr>::ColumnName;
        static_assert(!col.empty(), "Where<MP>: member not mapped to a column in this table.");
        return push_where(std::string(col), "=", std::forward<V>(value));
    }

    /// WHERE column op value  (custom operator: "<", ">", "!=", "LIKE", …)
    template <auto MemberPtr, typename V>
    [[nodiscard]] SelectQuery whereOp(std::string_view op, V&& value) &&
    {
        constexpr std::string_view col = TableType::template ColumnOf<MemberPtr>::ColumnName;
        static_assert(!col.empty(), "WhereOp<MP>: member not mapped to a column.");
        return push_where(std::string(col), std::string(op), std::forward<V>(value));
    }

    // ----------------------------------------------------------------
    //  WHERE - runtime member pointer (fallback; same column resolution,
    //          but done at runtime, so produces a slightly slower path)
    // ----------------------------------------------------------------
    template <typename Field, typename Obj, typename V>
    [[nodiscard]] SelectQuery where(Field Obj::* mp, V&& value) &&
    {
        const std::string col = runtime_col_name(mp);
        if (col.empty()) throw engine::SqliteError("Where(mp, v): member not mapped to any column.");
        return push_where(col, "=", std::forward<V>(value));
    }

    // ----------------------------------------------------------------
    //  Join<TargetTable>()
    //
    //  Automatically detects the FK relationship between TableType and
    //  TargetTable.  Checks:
    //    1. TableType has a FK column pointing to TargetTable  (forward FK)
    //    2. TargetTable has a FK column pointing to TableType  (reverse FK)
    //  A static_assert fires if neither direction has a FK.
    //  A static_assert fires if both directions have a FK
    //  (ambiguous - use JoinOn<> explicitly in that case).
    // ----------------------------------------------------------------
    template <typename TargetTable>
    requires IsTable<TargetTable>
    [[nodiscard]] auto join() &&
    {
        using LT = TableType;
        using RT = TargetTable;

        constexpr bool fwd = detail::HasFkTo<LT, RT>; // LT.col -> RT
        constexpr bool rev = detail::HasFkTo<RT, LT>; // RT.col -> LT

        static_assert(fwd || rev, "Join<T>: no ForeignKey<> found between these two tables. "
                                  "Use JoinOn<&L::col, &R::col>() for an explicit condition.");

        static_assert(!(fwd && rev),
                      "Join<T>: ForeignKey exists in both directions - relationship is ambiguous. "
                      "Use JoinOn<&L::col, &R::col>() to specify which to use.");

        // TODO: Replace with structured bindings when the constexpr support is implemented
        if constexpr (fwd)
        {
            constexpr auto columnPair = detail::resolve_fk_forward<LT, RT>();
            return JoinQuery<LT, RT>(_db, std::move(_state), columnPair.first, columnPair.second);
        }
        else
        {
            constexpr auto columnPair = detail::resolve_fk_reverse<LT, RT>();
            return JoinQuery<LT, RT>(_db, std::move(_state), columnPair.first, columnPair.second);
        }
    }

    // ----------------------------------------------------------------
    //  ORDER BY
    // ----------------------------------------------------------------
    template <auto MemberPtr>
    [[nodiscard]] SelectQuery orderBy(Order order = Order::ASC) &&
    {
        constexpr std::string_view col = TableType::template ColumnOf<MemberPtr>::ColumnName;
        static_assert(!col.empty(), "OrderBy<MP>: member not mapped to a column.");

        _state.order_clauses.emplace_back(std::string(col), order);
        return std::move(*this);
    }

    // ----------------------------------------------------------------
    // Full-row DISTINCT  - SELECT DISTINCT col1, col2, … FROM table
    // ----------------------------------------------------------------
    [[nodiscard]] SelectQuery distinct() &&
    {
        _state.distinct = true;
        return std::move(*this);
    }

    // ----------------------------------------------------------------
    // Single-column DISTINCT  - SELECT DISTINCT col FROM table
    // Returns a QueryRange<TableType> where each row is hydrated from
    // a single column.  Works best combined with ToVector() or range-for.
    // ----------------------------------------------------------------
    template <auto MemberPtr>
    [[nodiscard]] SelectQuery distinct() &&
    {
        constexpr std::string_view col = TableType::template columnNameOf<MemberPtr>();
        static_assert(!col.empty(),
                      "Distinct<MP>: member pointer not mapped to any column in this table.");

        _state.distinct     = true;
        _state.distinct_col = std::string(col);
        return std::move(*this);
    }

    // ----------------------------------------------------------------
    //  LIMIT / OFFSET
    // ----------------------------------------------------------------
    [[nodiscard]] SelectQuery limit(i32 n) &&
    {
        _state.limit = n;
        return std::move(*this);
    }

    [[nodiscard]] SelectQuery offset(i32 n) &&
    {
        _state.offset_val = n;
        return std::move(*this);
    }

    // ----------------------------------------------------------------
    //  Terminal operations
    // ----------------------------------------------------------------

    /// Returns a lazy QueryRange (query runs only when iterated).
    [[nodiscard]] QueryRange<TableType> asRange() &&
    {
        return QueryRange<TableType>(_db, std::move(_state));
    }

    /// Materialise all results.
    [[nodiscard]] std::vector<RowType> toVector() &&
    {
        return std::move(*this).asRange().toVector();
    }

    /// Return first result or std::nullopt.
    [[nodiscard]] std::optional<RowType> firstOrDefault() &&
    {
        return std::move(*this).limit(1).asRange().firstOrDefault();
    }

    /// Return first result or fallback value.
    template <typename U = std::remove_cv_t<RowType>>
    [[nodiscard]] std::optional<RowType> firstOrDefault(U&& fallback) &&
    {
        return std::move(*this).limit(1).asRange().firstOrDefault(std::forward<U>(fallback));
    }

    // ----------------------------------------------------------------
    //  range-for support (iterating a SelectQuery directly)
    // ----------------------------------------------------------------
    auto begin()
    {
        if (!_range)
        {
            _range.emplace(_db, _state);
        }
        return _range->begin();
    }
    auto end()
    {
        if (!_range)
        {
            _range.emplace(_db, _state);
        }
        return _range->end();
    }

    // Expose state for Repository::CountWhere / DeleteWhere
    [[nodiscard]] const detail::QueryState& state() const noexcept { return _state; }

private:
    engine::SqliteConnection& _db;
    detail::QueryState _state;
    std::optional<QueryRange<TableType>> _range;

    // Append a predicate and return a new builder (value semantics).
    template <typename V>
    requires SqliteAdaptable<V>
    [[nodiscard]] SelectQuery push_where(std::string col, std::string op, V&& value)
    {
        using RV    = std::remove_cvref_t<V>;
        RV captured = std::forward<V>(value);

        detail::WherePredicate pred{.column = std::move(col),
                                    .op     = std::move(op),
                                    .binder = [captured](engine::SqliteStatement& stmt, i32 idx)
        {
            SqliteTypeAdapter<RV>::bind(stmt, idx, captured);
        }};

        _state.predicates.push_back(std::move(pred));
        return std::move(*this);
    }

    // Runtime member-pointer → column name.
    template <typename Field, typename Obj>
    [[nodiscard]] std::string runtime_col_name(Field Obj::* mp) const
    {
        std::string result;
        std::apply([&](auto... cols_v)
        {
            ([&]<typename Col>(Col)
            {
                if constexpr (std::is_same_v<decltype(Col::MemberPtr), Field Obj::*>)
                {
                    if (Col::MemberPtr == mp)
                    {
                        result = std::string(Col::ColumnName);
                    }
                }
            }(cols_v), ...);
        }, typename TableType::ColumnsTuple{});
        return result;
    }
};
} // namespace tewi
