module tewi:row_hydrator;

import :table;
import :member_traits;
import :type_adapter;
import :sqlite_statement;

import std;

namespace tewi::detail
{
template <typename TResult>
concept QueryHydrator = requires(const engine::SqliteStatement& stmt, i32 offset) {
    { TResult{} };
};

template <ITable TableType>
struct table_hydrator
{
    using TResult = TableType::RowType;

    [[nodiscard]] TResult operator()(const engine::SqliteStatement& stmt, i32 offset = 0) const
    {
        return TableType::hydrate(stmt, offset);
    }
};

template <ITable LTable, ITable RTable>
struct pair_hydrator
{
    using TResult = std::pair<typename LTable::RowType, typename RTable::RowType>;

    [[nodiscard]] TResult operator()(const engine::SqliteStatement& stmt, i32 offset = 0) const
    {
        return {LTable::hydrate(stmt, offset),
                RTable::hydrate(stmt, offset + static_cast<i32>(LTable::columnsCount))};
    }
};

template <typename T>
struct type_hydrator
{
    [[nodiscard]] T operator()(const engine::SqliteStatement& stmt, i32 offset = 0) const
    {
        return SqliteTypeAdapter<T>::read(stmt, offset);
    }
};

template <typename... Ts>
struct type_hydrator<std::tuple<Ts...>>
{
    [[nodiscard]] std::tuple<Ts...> operator()(const engine::SqliteStatement& stmt,
                                               i32 offset = 0) const
    {
        return [&]<std::size_t... Is>(std::index_sequence<Is...>)
        {
            return std::tuple<Ts...>{
                SqliteTypeAdapter<Ts>::read(stmt, offset + static_cast<i32>(Is))...};
        }(std::index_sequence_for<Ts...>{});
    }
};

template <auto... MemberPtrs>
using projection_hydrator = type_hydrator<ProjectionResult<MemberPtrs...>>;

template <auto... MemberPtrs>
[[nodiscard]] constexpr auto make_projection_hydrator()
{
    return projection_hydrator<MemberPtrs...>{};
}

template <ITable TableType>
[[nodiscard]] constexpr auto make_table_hydrator()
{
    return table_hydrator<TableType>{};
}

template <ITable LTable, ITable RTable>
[[nodiscard]] constexpr auto make_pair_hydrator()
{
    return pair_hydrator<LTable, RTable>{};
}
} // namespace tewi::detail
