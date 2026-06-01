#pragma once

/**
 * @brief Registers a row type with its corresponding table descriptor.
 *
 * Expands to a specialization of @c tewi::detail::TableRegistry<RowType> so
 * that ORM facilities can resolve the table descriptor associated with a row
 * type at compile time. This enables APIs such as
 * @c OrmDatabase::select<RowType>() to infer the correct table metadata.
 *
 * Place this macro at namespace scope after the table alias definition.
 *
 * @param RowType Plain row struct to register.
 * @param TableAlias Table descriptor type associated with @p RowType.
 *
 * @note @c RowType must be a plain C++ data type and @c TableAlias must
 *       be a @c Table<> descriptor.
 * @warning This macro must be used at namespace scope.
 */
#define TEWI_REGISTER_TABLE(RowType, TableAlias)                     \
static_assert(!::tewi::detail::IsTable<RowType>,                     \
              "TEWI_REGISTER_TABLE: RowType must be a plain struct, "\
              "not a Table<> descriptor.");                          \
static_assert(::tewi::detail::IsTable<TableAlias>,                   \
             "TEWI_REGISTER_TABLE: TableAlias must be a Table<> "    \
             "descriptor.");                                         \
template <>                                                          \
struct tewi::detail::TableRegistry<RowType>                          \
{                                                                    \
    using TableType = TableAlias;                                    \
};
