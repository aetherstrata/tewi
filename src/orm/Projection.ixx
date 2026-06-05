export module tewi:projection;

import :member_traits;
import :fk_helpers;
import :query_state;
import :registry;
import :sqlite_connection;
import :table;
import :type_adapter;

import std;

// ============================================================================
//  §13 Multi-column projection
// ============================================================================
export namespace tewi
{
template <typename TableType, auto... MemberPtrs>
requires IsTable<TableType> && detail::HomogeneousMemberPtrs<MemberPtrs...>
class ColumnProjectionQuery
{
public:
    using result_tuple = detail::MembersTuple<MemberPtrs...>;

    explicit ColumnProjectionQuery(engine::SqliteConnection& db, detail::QueryState state = {})
        : _db(db)
        , _state(std::move(state))
    {}

    // ----------------------------------------------------------------
    //  WHERE / OrderBy / Limit  - identical pattern to SelectQuery
    // ----------------------------------------------------------------
    template <auto MP, typename V>
    [[nodiscard]] ColumnProjectionQuery where(V&& value) &&
    {
        constexpr std::string_view col = TableType::template ColumnOf<MP>::ColumnName;
        static_assert(!col.empty(), "Where<MP>: member not mapped.");
        return push_where(std::string(col), "=", std::forward<V>(value));
    }

    template <auto MP, typename V>
    [[nodiscard]] ColumnProjectionQuery whereOp(std::string_view op, V&& value) &&
    {
        constexpr std::string_view col = TableType::template columnNameOf<MP>();
        static_assert(!col.empty());
        return push_where(std::string(col), std::string(op), std::forward<V>(value));
    }

    template <auto MP>
    [[nodiscard]] ColumnProjectionQuery orderBy(Order ord = Order::ASC) &&
    {
        constexpr std::string_view col = TableType::template columnNameOf<MP>();
        static_assert(!col.empty());
        _state.order_clauses.emplace_back(std::string(col), ord);
        return std::move(*this);
    }

    [[nodiscard]] ColumnProjectionQuery limit(i32 n) &&
    {
        auto query         = *this;
        query._state.limit = n;
        return query;
    }

    [[nodiscard]] ColumnProjectionQuery offset(i32 n) &&
    {
        auto query              = *this;
        query._state.offset_val = n;
        return query;
    }

    [[nodiscard]] ColumnProjectionQuery distinct() &&
    {
        auto query            = *this;
        query._state.distinct = true;
        return query;
    }

    // ----------------------------------------------------------------
    //  Join<TargetTable>()  - auto-FK, same logic as SelectQuery::Join
    //  Returns a JoinProjectionQuery (see §3).
    // ----------------------------------------------------------------
    template <typename TargetTable>
        requires IsTable<TargetTable>
    [[nodiscard]] auto join() &&; // defined after JoinProjectionQuery

    // ----------------------------------------------------------------
    //  Join<MPs...>()  - column-projected join
    //  Adds columns from TargetTable (deduced from MPs) to the result.
    // ----------------------------------------------------------------
    template <auto... RightMPs>
        requires detail::HomogeneousMemberPtrs<RightMPs...>
    [[nodiscard]] auto join() &&; // defined after JoinProjectionQuery

    // ----------------------------------------------------------------
    //  Terminal operations
    // ----------------------------------------------------------------
    [[nodiscard]] std::vector<result_tuple> toVector()
    {
        auto stmt = _db.prepare(build_sql());
        _state.bind_where(stmt);
        std::vector<result_tuple> out;
        while (stmt.step()) out.push_back(hydrate(stmt));
        return out;
    }

    [[nodiscard]] std::optional<result_tuple> firstOrDefault()
    {
        auto query = std::move(*this).Limit(1);
        auto stmt  = _db.prepare(query.build_sql());
        query._state.bind_where(stmt);
        if (!stmt.step()) return std::nullopt;
        return query.hydrate(stmt);
    }

    template <typename U = std::remove_cv_t<result_tuple>>
    [[nodiscard]] result_tuple firstOrDefault(U&& fallback)
    {
        auto query = std::move(*this).Limit(1);
        auto stmt  = _db.prepare(query.build_sql());
        query._state.bind_where(stmt);
        if (!stmt.step()) return static_cast<result_tuple>(std::forward<U>(fallback));
        return query.hydrate(stmt);
    }

    /// @name range-for support
    /// @{

    struct Sentinel {};

    class Iterator
    {
    public:
        Iterator() = default;
        Iterator(engine::SqliteConnection& db, const detail::QueryState& state, const std::string& sql)
        {
            _stmt = std::make_shared<engine::SqliteStatement>(db.Handle(), sql);
            state.bind_where(*_stmt);
            fetch();
        }
        const result_tuple& operator*() const { return *_cur; }
        const result_tuple* operator->() const { return &*_cur; }
        Iterator& operator++()
        {
            fetch();
            return *this;
        }
        bool operator==(Sentinel) const { return !_cur.has_value(); }

    private:
        std::shared_ptr<engine::SqliteStatement> _stmt;
        std::optional<result_tuple> _cur;
        void fetch()
        {
            if (_stmt && _stmt->step())
            {
                _cur = hydrate_static(*_stmt);
            }
            else
            {
                _cur.reset();
            }
        }
        // Static version for use inside Iterator (no access to outer `this`)
        static result_tuple hydrate_static(const engine::SqliteStatement& stmt)
        {
            return hydrate_impl(stmt, std::index_sequence_for<decltype(MemberPtrs)...>{});
        }
        template <usize... Is>
        static result_tuple hydrate_impl(const engine::SqliteStatement& stmt,
                                         std::index_sequence<Is...>)
        {
            return {read_col<std::tuple_element_t<Is, result_tuple>>(stmt, Is)...};
        }
    };
    /// @}

    Iterator begin() { return Iterator(_db, _state, build_sql()); }
    Sentinel end() { return {}; }

    [[nodiscard]] const detail::QueryState& state() const noexcept { return _state; }

private:
    engine::SqliteConnection& _db;
    detail::QueryState _state;

    // Build SELECT col1, col2, ... FROM table WHERE ... ORDER BY ... LIMIT ...
    [[nodiscard]] std::string build_sql() const
    {
        return "SELECT " + std::string(_state.distinct ? "DISTINCT " : "") + column_list()
               + " FROM " + std::string(TableType::TableName) + _state.where_sql()
               + _state.order_sql() + _state.limit_sql() + ";";
    }

    // Comma-separated column names for the projected MPs.
    [[nodiscard]] static std::string column_list()
    {
        std::string sql;
        bool first = true;
        ([&]<auto MP>()
        {
            constexpr std::string_view col = TableType::template columnNameOf<MP>();
            static_assert(!col.empty(), "Select<MP>: member not mapped.");
            if (!first) sql += ", ";
            sql   += std::string(TableType::TableName) + "." + std::string(col);
            first  = false;
        }.template operator()<MemberPtrs>(), ...);
        return sql;
    }

    // Hydrate a result_tuple from a statement row.
    [[nodiscard]] static result_tuple hydrate(const engine::SqliteStatement& stmt)
    {
        return hydrate_idx(stmt, std::index_sequence_for<decltype(MemberPtrs)...>{});
    }

    template <usize... Is>
    [[nodiscard]] static result_tuple hydrate_idx(const engine::SqliteStatement& stmt,
                                                  std::index_sequence<Is...>)
    {
        return {read_col<std::tuple_element_t<Is, result_tuple>>(stmt, static_cast<i32>(Is))...};
    }

    // Read a single column by its C++ type.
    template <typename FT>
    requires SqliteAdaptable<FT>
    [[nodiscard]] static FT read_col(const engine::SqliteStatement& stmt, i32 idx)
    {
        return SqliteTypeAdapter<FT>::read(stmt, idx);
    }

    template <typename V>
    requires SqliteAdaptable<V>
    [[nodiscard]] ColumnProjectionQuery push_where(std::string col, std::string op, V&& value)
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
};

// Tag wrappers that carry a MP pack as a single template argument.
template <auto... MPs>
struct LeftCols {};

template <auto... MPs>
struct RightCols {};

template <typename LTable, typename RTable,
          typename LeftPack, typename RightPack>
class JoinProjectionQuery;

// Carries left MPs and right MPs separately so the SQL column list
// can prefix each with its owning table name.
template <typename LTable, typename RTable, auto... LMPs, auto... RMPs>
class JoinProjectionQuery<LTable, RTable, LeftCols<LMPs...>, RightCols<RMPs...>>
{
public:
    using left_tuple  = detail::MembersTuple<LMPs...>;
    using right_tuple = detail::MembersTuple<RMPs...>;
    // Combined result: std::tuple<L fields..., R fields...>
    using result_tuple =
        decltype(std::tuple_cat(std::declval<left_tuple>(), std::declval<right_tuple>()));

    JoinProjectionQuery(engine::SqliteConnection& db, detail::QueryState state,
                        std::string_view left_col, std::string_view right_col)
        : _db(db)
        , _state(std::move(state))
        , _left_col(left_col)
        , _right_col(right_col)
    {}

    // WHERE / OrderBy / Limit - same pattern, omitted for brevity

    [[nodiscard]] std::vector<result_tuple> toVector()
    {
        auto stmt = _db.prepare(build_sql());
        _state.bind_where(stmt);
        std::vector<result_tuple> out;
        while (stmt.step()) out.push_back(hydrate(stmt));
        return out;
    }

    // Further Join<MPs...>() would extend to a three-table variant - see §4.

private:
    engine::SqliteConnection& _db;
    detail::QueryState _state;
    std::string_view _left_col;
    std::string_view _right_col;

    [[nodiscard]] std::string build_sql() const
    {
        return "SELECT " + col_list<LTable, LMPs...>() + ", " + col_list<RTable, RMPs...>()
               + " FROM " + std::string(LTable::TableName) + " INNER JOIN "
               + std::string(RTable::TableName) + " ON " + std::string(LTable::TableName) + "."
               + std::string(_left_col) + " = " + std::string(RTable::TableName) + "."
               + std::string(_right_col) + _state.where_sql() + _state.order_sql()
               + _state.limit_sql() + ";";
    }

    template <typename T, auto... MPs>
    static std::string col_list()
    {
        std::string sql;
        bool first = true;
        ([&]<auto MP>()
        {
            constexpr std::string_view col = T::template columnNameOf<MP>();
            static_assert(!col.empty());
            if (!first) sql += ", ";
            sql   += std::string(T::TableName) + "." + std::string(col);
            first  = false;
        }.template operator()<MPs>(), ...);
        return sql;
    }

    [[nodiscard]] static result_tuple hydrate(const engine::SqliteStatement& stmt)
    {
        // Read left columns starting at 0, right columns starting at sizeof...(LMPs)
        auto left  = hydrate_pack<left_tuple, LMPs...>(stmt, 0);
        auto right = hydrate_pack<right_tuple, RMPs...>(stmt, sizeof...(LMPs));
        return std::tuple_cat(left, right);
    }

    template <typename TupleT, auto... MPs>
    static TupleT hydrate_pack(const engine::SqliteStatement& stmt, i32 offset)
    {
        return hydrate_idx<TupleT>(stmt, offset, std::index_sequence_for<decltype(MPs)...>{});
    }

    template <typename TupleT, usize... Is>
    static TupleT hydrate_idx(const engine::SqliteStatement& stmt, i32 offset,
                              std::index_sequence<Is...>)
    {
        return {ColumnProjectionQuery<LTable, LMPs...>::template read_col<
            std::tuple_element_t<Is, TupleT>>(stmt, static_cast<i32>(offset + Is))...};
    }
};

// Auto-FK join - all columns of TargetTable
template <typename TableType, auto... MemberPtrs> // on ColumnProjectionQuery
requires IsTable<TableType> && detail::HomogeneousMemberPtrs<MemberPtrs...>
template <typename TargetTable>
requires IsTable<TargetTable>
[[nodiscard]] auto ColumnProjectionQuery<TableType, MemberPtrs...>::join() &&
{
    // Same FK detection logic as SelectQuery::Join<> (§3 of the auto-join design)
    constexpr bool fwd = detail::HasFkTo<TableType, TargetTable>;
    constexpr bool rev = detail::HasFkTo<TargetTable, TableType>;
    static_assert(fwd || rev, "Join<T>: no FK found.");
    static_assert(!(fwd && rev), "Join<T>: ambiguous FK.");

    // Resolve ON clause columns and expand all RTable columns into the MP pack
    if constexpr (fwd)
    {
        constexpr auto columnPair = detail::resolve_fk_forward<TableType, TargetTable>();
        return make_join_all_cols<TargetTable>(columnPair.fist, columnPair.second, typename TargetTable::ColumnsTuple{});
    }
    else
    {
        constexpr auto columnPair = detail::resolve_fk_reverse<TableType, TargetTable>();
        return make_join_all_cols<TargetTable>(columnPair.first, columnPair.second, typename TargetTable::ColumnsTuple{});
    }
}

// Column-projected join: .Join<&Leaderboard::time, &Leaderboard::score>()
template <typename TableType, auto... MemberPtrs>
    requires IsTable<TableType> && detail::HomogeneousMemberPtrs<MemberPtrs...>
template <auto... RightMPs>
    requires detail::HomogeneousMemberPtrs<RightMPs...>
[[nodiscard]] auto ColumnProjectionQuery<TableType, MemberPtrs...>::join() &&
{
    using RObj = detail::projection_object_t<RightMPs...>;
    static_assert(HasRegisteredTable<RObj>, "Join<MPs...>: right-side row type not registered.");

    using RT = TableRegistry<RObj>::TableType;

    constexpr bool fwd = detail::HasFkTo<TableType, RT>;
    constexpr bool rev = detail::HasFkTo<RT, TableType>;

    static_assert(fwd || rev, "Join<MPs...>: no FK found between tables.");
    static_assert(!(fwd && rev), "Join<MPs...>: ambiguous FK.");

    if constexpr (fwd)
    {
        constexpr auto columnPair = detail::resolve_fk_forward<TableType, RT>();
        return JoinProjectionQuery<TableType, RT, LeftCols<MemberPtrs...>, RightCols<RightMPs...>>
        (_db, std::move(_state), columnPair.fist, columnPair.second);
    }
    else
    {
        constexpr auto columnPair = detail::resolve_fk_reverse<TableType, RT>();
        return JoinProjectionQuery<TableType, RT, LeftCols<MemberPtrs...>, RightCols<RightMPs...>>
        (_db, std::move(_state), columnPair.first, columnPair.second);
    }
}
} // namespace tewi
