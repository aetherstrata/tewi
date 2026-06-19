export module tewi:table_helpers;

import :table;

namespace tewi
{
namespace detail
{
template <typename T>
struct is_table : std::false_type
{};

template <FixedString name, typename T, typename ColPack, typename IdxPack>
struct is_table<Table<name, T, ColPack, IdxPack>> : std::true_type
{};
} // namespace detail

export template <typename T>
concept ITable = detail::is_table<T>::value;

// Requires the table to have one primary key
template <typename TableType>
concept HasPrimaryKey = TableType::primaryKeyCount > 0;
} // namespace tewi::detail
