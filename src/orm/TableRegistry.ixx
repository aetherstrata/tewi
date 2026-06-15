export module tewi:registry;

import std;
import :table;

// ============================================================================
//  TableRegistry  - maps Row types to their Table descriptors
// ============================================================================

export namespace tewi
{
/**
 * @brief Compile-time registry that maps a row type to its table descriptor type
 *
 * This primary template acts as the internal lookup point used to associate a
 * user-defined row type with its corresponding @c Table<> descriptor. By default,
 * no table is registered, so @ref TableType resolves to @c void .
 *
 * @note Specializations are typically generated through the public
 *       `ORM_REGISTER_TABLE(RowType, TableAlias)` macro rather than
 *       written manually.
 *
 * @tparam RowType Plain row struct queried by the ORM.
 */
template <typename RowType>
struct TableRegistry
{
    /**
     * @brief Registered table descriptor type for @p RowType.
     *
     * The primary template uses @c void to indicate that no table
     * mapping has been registered for the row type.
     */
    using TableType = void;
};

/**
 * @brief Checks whether a row type has a registered table mapping.
 *
 * This concept evaluates to @c true when the @ref TableRegistry for the given
 * row type provides a non @c void @c TableType, which indicates that a table
 * descriptor has been registered.
 *
 * @note Registration is normally performed with the public
 *       `ORM_REGISTER_TABLE(RowType, TableAlias)` macro.
 *
 * @tparam RowType Row type to test.
 */
template <typename RowType>
concept HasRegisteredTable = !std::is_void_v<typename TableRegistry<RowType>::TableType>;
} // namespace tewi
