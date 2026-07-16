// ============================================================================
// BasicQuery.ixx  -  tewi:basic_query
//
// The single query class that replaces SelectQuery, JoinQuery, and
// ColumnProjectionQuery.  It owns a SelectSpec (semantic AST) and a pluggable
// THydrator.  SqliteRenderer is the only thing that touches SQL text.
// ============================================================================
export module tewi:basic_query;

import :ast_spec;
import :ast_renderer;
import :ast_compiled;
import :row_hydrator;
import :sqlite_connection;
import :sqlite_statement;
import :table_helpers;
import :registry;
import :member_traits;
import :fk_helpers;
import :order;
import :type_adapter;

import std;

namespace tewi
{
// -----------------------------------------------------------------------
// Helper: build fully-qualified column name from member pointer + table pack
// -----------------------------------------------------------------------
namespace detail
{
/// Table<> descriptor owning MemberPtr's object type; void when unregistered.
template <auto MemberPtr>
using OwnerTable = TableRegistry<ObjectOf<MemberPtr>>::TableType;

/// True when MemberPtr's table is among Tables... and maps the member to a column.
template <auto MemberPtr, ITable... Tables>
constexpr bool isQueriedMember = []
{
    if constexpr (HasRegisteredTable<ObjectOf<MemberPtr>>)
    {
        return (std::is_same_v<OwnerTable<MemberPtr>, Tables> || ...)
               && !std::is_void_v<typename OwnerTable<MemberPtr>::template ColumnOf<MemberPtr>>;
    }
    else return false;
}();

template <auto MemberPtr, ITable... Tables>
[[nodiscard]] constexpr std::string_view qualified_column_name()
{
    static_assert(isQueriedMember<MemberPtr, Tables...>,
                  "qualified_column_name: member pointer must map to a column of one of the "
                  "queried tables, and its owner type must be registered with ORM_REGISTER_TABLE.");

    if constexpr (isQueriedMember<MemberPtr, Tables...>)
    {
        using Owner = OwnerTable<MemberPtr>;

        // Static storage ensures the string outlives the function return.
        static constexpr auto fqn =
            Owner::template fully_qualified_name<typename Owner::template ColumnOf<MemberPtr>>();

        return fqn.view();
    }
    else return {};
}

/// Build the SelectSpec projection column list for all-column queries.
template <ITable... Tables>
[[nodiscard]] ast::ProjectionNode make_all_columns_projection()
{
    ast::ProjectionNode proj;
    proj.kind = ast::ProjectionKind::ColumnList;
    ([&]<typename T>()
    {
        // Expand each table's columns as "tableName.colName"
        std::apply([&](auto... col_tags)
        {
            ([&]<typename ColType>(ColType col_tag)
            {
                proj.columns.emplace_back(std::string(T::tableName) + "."
                                        + std::string(ColType::columnName));
            }(col_tags), ...);
        }, typename T::ColumnsTuple{});
    }.template operator()<Tables>(), ...);
    return proj;
}

/// Build a TableRef for the primary (FROM) table.
template <ITable TableType>
[[nodiscard]] ast::TableRef make_table_ref()
{
    return ast::TableRef{
        .name            = std::string(TableType::tableName),
        .all_columns_sql = TableType::column_list(TableType::tableName)
    };
}

/// Build a JoinNode from explicit ON columns.
template <ITable LTable, ITable RTable>
[[nodiscard]] ast::JoinNode make_join_node(std::string_view left_col, std::string_view right_col,
                                           ast::JoinKind kind = ast::JoinKind::Inner)
{
    return ast::JoinNode{
        .kind       = kind,
        .table      = make_table_ref<RTable>(),
        .left_qual  = std::string(LTable::tableName) + "." + std::string(left_col),
        .right_qual = std::string(RTable::tableName) + "." + std::string(right_col)
    };
}

} // namespace detail

// =========================================================================
// BasicQuery<TResult, THydrator, Tables...>
// =========================================================================

/// Unified query builder.  All filter/order/limit state is stored as typed
/// AST nodes in a SelectSpec; SQL is only produced at execution time by
/// SqliteRenderer.
export template <typename TResult, typename THydrator, ITable... Tables>
class BasicQuery
{
    static_assert(sizeof...(Tables) >= 1, "BasicQuery requires at least one table.");

    using FirstTable = std::tuple_element_t<0, std::tuple<Tables...>>;

public:
    using result_type   = TResult;
    using hydrator_type = THydrator;

    explicit BasicQuery(engine::SqliteConnection& db, ast::SelectSpec spec, THydrator hydrator = {})
        : _db(db)
        , _spec(std::move(spec))
        , _hydrator(std::move(hydrator))
    {}

    // ----------------------------------------------------------------
    // WHERE  column = value  (NTTP member pointer)
    // ----------------------------------------------------------------
    template <auto MemberPtr, typename V>
    [[nodiscard]] BasicQuery where(V&& value) &&
    {
        return std::move(*this).template where<MemberPtr>(Compare::Equal, std::forward<V>(value));
    }

    // ----------------------------------------------------------------
    // WHERE  column OP value  (custom operator)
    // ----------------------------------------------------------------
    template <auto MemberPtr, typename V>
    [[nodiscard]] BasicQuery where(Compare op, V&& value) &&
    {
        static_assert(std::is_convertible_v<std::remove_cvref_t<V>, detail::RemoveOptional<detail::FieldOf<MemberPtr>>>,
            "where<MemberPtr>: value type is not compatible with the member pointer's field type.");

        const std::string_view col = detail::qualified_column_name<MemberPtr, Tables...>();

        return std::move(*this).template push_predicate(col, op, std::forward<V>(value));
    }

    // ----------------------------------------------------------------
    // ORDER BY
    // ----------------------------------------------------------------
    template <auto MemberPtr>
    [[nodiscard]] BasicQuery orderBy(Order ord = Order::ASC) &&
    {
        _spec.order_by.emplace_back(ast::OrderNode{
            .column = std::string(detail::qualified_column_name<MemberPtr, Tables...>()),
            .direction = ord
        });
        return std::move(*this);
    }

    // ----------------------------------------------------------------
    // LIMIT / OFFSET
    // ----------------------------------------------------------------
    [[nodiscard]] BasicQuery limit(i32 n) &&
    {
        _spec.limit = n;
        return std::move(*this);
    }

    [[nodiscard]] BasicQuery offset(i32 n) &&
    {
        _spec.offset = n;
        return std::move(*this);
    }

    // ----------------------------------------------------------------
    // DISTINCT (full-row)
    // ----------------------------------------------------------------
    [[nodiscard]] BasicQuery distinct() &&
    {
        _spec.projection.distinct = true;
        return std::move(*this);
    }

    // ----------------------------------------------------------------
    // DISTINCT (single column)
    // ----------------------------------------------------------------
    template <auto MemberPtr>
    [[nodiscard]] BasicQuery distinct() &&
    {
        _spec.projection.distinct     = true;
        _spec.projection.distinct_col = detail::qualified_column_name<MemberPtr, Tables...>();
        return std::move(*this);
    }

    // ----------------------------------------------------------------
    // Auto-FK JOIN (adds RTable to the source scope)
    // ----------------------------------------------------------------
    template <ITable TargetTable>
    [[nodiscard]] auto join() &&
    {
        static_assert(sizeof...(Tables) == 1,
                      "join<TargetTable>() currently requires a single-table base query. "
                      "Use joinOn<LMp,RMp>() for explicit multi-table joins.");

        using LT = FirstTable;
        using RT = TargetTable;

        constexpr bool fwd = LT::template hasFkTo<RT>;
        constexpr bool rev = RT::template hasFkTo<LT>;
        static_assert(fwd || rev,
                      "join<TargetTable>: no ForeignKey<> found between the two tables. "
                      "Use joinOn<&L::col, &R::col>().");
        static_assert(!(fwd && rev), "join<TargetTable>: FK exists in both directions - ambiguous. "
                                     "Use joinOn<&L::col, &R::col>().");

        if constexpr (fwd)
        {
            constexpr auto pair = detail::resolve_fk_forward<LT, RT>();
            ast::JoinNode jn    = detail::make_join_node<LT, RT>(pair.first, pair.second);
            _spec.joins.emplace_back(std::move(jn));
        }
        else
        {
            constexpr auto pair = detail::resolve_fk_reverse<LT, RT>();
            ast::JoinNode jn    = detail::make_join_node<LT, RT>(pair.first, pair.second);
            _spec.joins.emplace_back(std::move(jn));
        }

        // Extend projection to include RT columns
        for (const auto& col : detail::make_all_columns_projection<RT>().columns)
        {
            _spec.projection.columns.emplace_back(col);
        }

        return BasicQuery<std::pair<typename LT::RowType, typename RT::RowType>,
                          detail::pair_hydrator<LT, RT>, LT, RT>
        (_db, std::move(_spec), detail::make_pair_hydrator<LT, RT>());
    }

    // ----------------------------------------------------------------
    // Explicit JOIN with ON from two member pointers
    // ----------------------------------------------------------------
    template <auto LeftMp, auto RightMp, ast::JoinKind Kind = ast::JoinKind::Inner>
    [[nodiscard]] auto joinOn() &&
    {
        using LObj = detail::ObjectOf<LeftMp>;
        using RObj = detail::ObjectOf<RightMp>;
        static_assert(HasRegisteredTable<LObj> && HasRegisteredTable<RObj>,
                      "joinOn<LMp,RMp>: both member owner types must be registered.");

        using LT = TableRegistry<LObj>::TableType;
        using RT = TableRegistry<RObj>::TableType;

        constexpr std::string_view lc = LT::template ColumnOf<LeftMp>::columnName;
        constexpr std::string_view rc = RT::template ColumnOf<RightMp>::columnName;
        static_assert(!lc.empty() && !rc.empty(),
                      "joinOn<LMp,RMp>: one or both members are not mapped to columns.");

        // Add RT columns to the projection
        for (const auto& col : detail::make_all_columns_projection<RT>().columns)
        {
            _spec.projection.columns.emplace_back(col);
        }

        _spec.joins.emplace_back(detail::make_join_node<LT, RT>(lc, rc, Kind));

        return BasicQuery<std::pair<typename LT::RowType, typename RT::RowType>,
                          detail::pair_hydrator<LT, RT>, LT, RT>
        (_db, std::move(_spec), detail::make_pair_hydrator<LT, RT>());
    }

    // ----------------------------------------------------------------
    // Narrow projection to a tuple of member values
    // ----------------------------------------------------------------
    template <auto... MemberPtrs>
    [[nodiscard]] auto select() &&
    {
        using ProjResult = detail::ProjectionResult<MemberPtrs...>;

        ast::ProjectionNode proj;
        proj.kind     = ast::ProjectionKind::ColumnList;
        proj.distinct = _spec.projection.distinct;
        ([&]<auto MP>()
        {
            constexpr std::string_view col = detail::qualified_column_name<MP, Tables...>();
            proj.columns.emplace_back(col);
        }.template operator()<MemberPtrs>(), ...);

        _spec.projection = std::move(proj);

        return BasicQuery<ProjResult, detail::projection_hydrator<MemberPtrs...>, Tables...>(
            _db, std::move(_spec), detail::make_projection_hydrator<MemberPtrs...>());
    }

    // ----------------------------------------------------------------
    // Terminal: materialise all rows
    // ----------------------------------------------------------------
    [[nodiscard]] std::vector<TResult> toVector() const
    {
        auto stmt = prepare();
        std::vector<TResult> out;
        while (stmt.step()) out.emplace_back(_hydrator(stmt, 0));
        return out;
    }

    // ----------------------------------------------------------------
    // Terminal: first row or nullopt
    // ----------------------------------------------------------------
    [[nodiscard]] std::optional<TResult> firstOrDefault()
    {
        _spec.limit = 1;
        auto stmt = prepare();
        if (!stmt.step()) return std::nullopt;
        return _hydrator(stmt, 0);
    }

    template <typename U = TResult>
    requires std::convertible_to<U, TResult>
    [[nodiscard]] TResult firstOrDefault(U&& fallback) const
    {
        auto opt = firstOrDefault();
        return opt.value_or(static_cast<TResult>(std::forward<U>(fallback)));
    }

    // ----------------------------------------------------------------
    // count() - count rows matching an existing BasicQuery's filters
    // ----------------------------------------------------------------
    [[nodiscard]] i64 count()
    {
        _spec.projection.kind     = ast::ProjectionKind::CountStar;
        _spec.projection.distinct = false;
        _spec.projection.columns.clear();
        _spec.order_by.clear(); // ORDER BY is meaningless for COUNT
        _spec.limit.reset();
        _spec.offset.reset();

        auto stmt = prepare();
        if (!stmt.step()) return 0;
        return SqliteTypeAdapter<i64>::read(stmt, 0);
    }

    // ----------------------------------------------------------------
    // Lazy range: begin / end
    // ----------------------------------------------------------------
    struct Sentinel {};

    class Iterator
    {
    public:
        using value_type        = TResult;
        using pointer           = const TResult*;
        using reference         = const TResult&;
        using difference_type   = std::ptrdiff_t;
        using iterator_category = std::input_iterator_tag;

        Iterator() = default;

        Iterator(engine::SqliteConnection& db, ast::CompiledShape cq, ast::BoundParams params, THydrator hydrator)
            : _stmt(std::make_shared<engine::SqliteStatement>(db.handle(), cq.sql()))
            , _hydrator(std::move(hydrator))
        {
            params.bind(*_stmt);
            fetch();
        }

        [[nodiscard]] reference operator*() const noexcept { return *_cur; }
        [[nodiscard]] pointer operator->() const noexcept { return &*_cur; }

        Iterator& operator++()
        {
            fetch();
            return *this;
        }

        [[nodiscard]] bool operator==(Sentinel) const noexcept { return !_cur.has_value(); }
        [[nodiscard]] bool operator!=(Sentinel s) const noexcept { return !(*this == s); }

    private:
        std::shared_ptr<engine::SqliteStatement> _stmt;
        THydrator _hydrator{};
        std::optional<TResult> _cur;

        void fetch()
        {
            if (_stmt && _stmt->step())
                _cur = _hydrator(*_stmt, 0);
            else
                _cur.reset();
        }
    };

    [[nodiscard]] Iterator begin() const
    {
        return Iterator(_db, ast::compile(_spec), _hydrator);
    }

    [[nodiscard]] Sentinel end() const noexcept { return {}; }

    // ----------------------------------------------------------------
    // Introspection (for Repository::countWhere etc.)
    // ----------------------------------------------------------------
    [[nodiscard]] const ast::SelectSpec& spec() const noexcept { return _spec; }

private:
    engine::SqliteConnection& _db;
    ast::SelectSpec _spec;
    ast::BoundParams _params;
    THydrator _hydrator{};
    i32 p_counter = 1;

    // Compile and prepare the statement in one step.
    [[nodiscard]] engine::SqliteStatement prepare() const
    {
        auto cq   = ast::compile(_spec);
        auto stmt = _db.prepare(cq.sql());
        _params.bind(stmt);
        return std::move(stmt);
    }

    // Append a typed predicate to the spec.
    template <typename V>
    [[nodiscard]] BasicQuery push_predicate(std::string_view col, Compare op, V&& value)
    {
        using RV = std::remove_cvref_t<V>;
        static_assert(SqliteAdaptable<RV>, "Value type is not adaptable to a SQLite type. "
                                           "Specialize SqliteTypeAdapter for it.");

        const std::string name = ast::makeParamName(p_counter);
        _spec.where.emplace_back(ast::PredicateNode {
            .column = std::string(col),
            .op     = op,
            .param_name = name
        });
        _params.add(name, std::forward<V>(value));
        p_counter++;
        return std::move(*this);
    }

    // Runtime member-pointer -> "table.col".
    template <typename Field, typename Obj>
    [[nodiscard]] std::string runtime_col_name(Field Obj::* mp) const
    {
        std::string result;

        ([&]<typename T>()
        {
            std::apply([&](auto... col_tags)
            {
                ([&]<typename ColType>(ColType col_tag)
                {
                    if constexpr (std::is_same_v<decltype(ColType::member), Field Obj::*>)
                    {
                        if (ColType::member == mp)
                        {
                            result = T::tableName + "." + ColType::columnName;
                        }
                    }
                }(col_tags), ...);
            }, typename T::ColumnsTuple{});
        }.template operator()<Tables>(), ...);

        return result;
    }
};

// -----------------------------------------------------------------------
// Factory helpers - called by OrmDatabase / Repository
// -----------------------------------------------------------------------

namespace detail
{
template <ITable TableType>
[[nodiscard]] auto make_basic_query(engine::SqliteConnection& db)
{
    ast::SelectSpec spec;
    spec.from       = make_table_ref<TableType>();
    spec.projection = make_all_columns_projection<TableType>();

    return BasicQuery<typename TableType::RowType, table_hydrator<TableType>, TableType>(
        db, std::move(spec), make_table_hydrator<TableType>());
}
} // namespace detail

} // namespace tewi
