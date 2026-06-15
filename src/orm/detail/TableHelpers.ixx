module tewi:table_helpers;

import std;

namespace tewi::detail
{
template <typename First, typename... Rest>
consteval bool unique_column_names()
{
    if constexpr (sizeof...(Rest) > 0)
    {
        return ((First::columnName != Rest::columnName) && ...) && unique_column_names<Rest...>();
    }
    else
        return true;
}

template <typename... Cols>
concept UniqueColumnNames = unique_column_names<Cols...>();
} // namespace tewi::detail
