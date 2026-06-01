#pragma once

/// Register a row type so that OrmDatabase::Select<RowType>() works.
/// Place this macro at namespace scope, after the Table alias definition.
#define ORM_REGISTER_TABLE(RowType, TableAlias)                      \
static_assert(!::tewi::detail::IsTable<RowType>,                \
              "ORM_REGISTER_TABLE: RowType must be a plain struct, " \
              "not a Table<> descriptor.");                          \
template <>                                                          \
struct tewi::detail::TableRegistry<RowType>                     \
{                                                                    \
    using TableType = TableAlias;                                    \
};
