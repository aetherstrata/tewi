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
 * @param TableAlias Table descriptor type associated with a plain struct.
 *
 * @note  @c TableAlias must be a @c Table<> descriptor.
 * @warning This macro must be used at global scope.
 */
#define TEWI_REGISTER_TABLE(TableAlias)                                              \
static_assert(::tewi::ITable<TableAlias>,                                            \
              "TEWI_REGISTER_TABLE: " #TableAlias " must be a Table<> descriptor."); \
template <> struct tewi::TableRegistry<typename TableAlias::RowType>                 \
{                                                                                    \
    using TableType = TableAlias;                                                    \
};
