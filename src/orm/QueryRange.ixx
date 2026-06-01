export module tewi:query_range;

import :constraint_helpers;
import :sqlite_connection;
import :query_state;

import std;

// ============================================================================
//  §11  QueryRange  - lazy input range
// ============================================================================
namespace tewi
{
/// A lazy input range that executes the compiled SQL only on first iteration.
template <typename TableType>
requires detail::IsTable<TableType>
class QueryRange
{
public:
    using RowType = TableType::RowType;

    QueryRange(engine::SqliteConnection& db, detail::QueryState state, std::string sql = {})
        : _db(db)
        , _state(std::move(state))
        , _explicit_sql(std::move(sql))
    {}

    // ------------------------------------------------------------------
    //  C++20 input iterator
    // ------------------------------------------------------------------
    struct Sentinel
    {};

    class Iterator
    {
    public:
        using value_type        = RowType;
        using pointer           = const RowType*;
        using reference         = const RowType&;
        using difference_type   = std::ptrdiff_t;
        using iterator_category = std::input_iterator_tag;

        Iterator() = default;

        Iterator(engine::SqliteConnection& db, const detail::QueryState& state, const std::string& sql)
        {
            _stmt = std::make_shared<engine::SqliteStatement>(db.Handle(), sql);
            state.bind_where(*_stmt);
            fetch_next();
        }

        [[nodiscard]] reference operator*() const noexcept { return *_current; }
        [[nodiscard]] pointer operator->() const noexcept { return &*_current; }

        Iterator& operator++()
        {
            fetch_next();
            return *this;
        }

        // Equality with sentinel (range-for end check)
        [[nodiscard]] bool operator==(Sentinel) const noexcept { return !_current.has_value(); }
        [[nodiscard]] bool operator!=(Sentinel s) const noexcept { return !(*this == s); }

        // Equality between two iterators (needed for std::ranges)
        [[nodiscard]] bool operator==(const Iterator& other) const noexcept
        {
            return !_current.has_value() && !other._current.has_value();
        }

    private:
        std::shared_ptr<engine::SqliteStatement> _stmt;
        std::optional<RowType> _current;

        void fetch_next()
        {
            if (_stmt && _stmt->step())
            {
                _current = TableType::hydrate(*_stmt);
            }
            else
            {
                _current.reset();
            }
        }
    };

    [[nodiscard]] Iterator begin() { return Iterator(_db, _state, get_sql()); }
    [[nodiscard]] Sentinel end() const noexcept { return {}; }

    /// Materialise all results into a vector.
    [[nodiscard]] std::vector<RowType> toVector()
    {
        std::vector<RowType> vec;
        for (auto& row : *this) vec.push_back(row);
        return vec;
    }

    /// Return the first row, or std::nullopt.
    [[nodiscard]] std::optional<RowType> firstOrDefault()
    {
        auto it = begin();
        if (it == Sentinel{}) return std::nullopt;
        return *it;
    }

    template <typename U = std::remove_cv_t<RowType>>
    [[nodiscard]] RowType firstOrDefault(U&& fallback)
    {
        auto it = begin();
        if (it == Sentinel{}) return static_cast<RowType>(std::forward<U>(fallback));
        return *it;
    }

private:
    engine::SqliteConnection& _db;
    detail::QueryState _state;
    std::string _explicit_sql;

    [[nodiscard]] std::string get_sql() const
    {
        if (!_explicit_sql.empty()) return _explicit_sql;

        const std::string distinct_kw = _state.distinct ? "DISTINCT " : "";

        // Single-column DISTINCT form
        if (_state.distinct_col.has_value())
            return "SELECT DISTINCT " + *_state.distinct_col + " FROM "
                   + std::string(TableType::TableName) + _state.where_sql() + _state.order_sql()
                   + _state.limit_sql() + ";";

        // Full-row DISTINCT (or plain SELECT)
        return "SELECT " + distinct_kw + TableType::column_list(TableType::TableName) + " FROM "
               + std::string(TableType::TableName) + _state.where_sql() + _state.order_sql()
               + _state.limit_sql() + ";";
    }
};
} // namespace tewi
