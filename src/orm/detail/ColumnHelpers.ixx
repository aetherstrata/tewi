module tewi:column_helpers;

import :column;

import std;

namespace tewi
{
namespace detail
{
template <typename T>
struct is_column : std::false_type
{};

template <FixedString name, auto MP, typename... Cs>
struct is_column<Column<name, MP, Cs...>> : std::true_type
{};
} // namespace detail

template <typename T>
concept IColumn = detail::is_column<T>::value;

namespace detail
{
template <IColumn First, IColumn... Rest>
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

} // namespace detail
} // namespace tewi
