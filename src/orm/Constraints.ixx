export module tewi:contraints;

import :fixed_string;

import std;

// ============================================================================
//  Column constraints
// ============================================================================

export namespace tewi
{
/**
 * @defgroup Constraints Column Constraints
 * @brief Compile-time constraint tags used to annotate column DDL definitions.
 *
 * Each tag in this group appends or encodes a specific SQL constraint
 * (e.g. `PRIMARY KEY`, `NOT NULL`, `CHECK`, `REFERENCES`) into the
 * column's generated DDL string.
 * @{
 */

/**
 * @brief Marks the column as the table's `PRIMARY KEY`.
 *
 * Appends either `PRIMARY KEY AUTOINCREMENT` or `PRIMARY KEY` to the
 * column's DDL definition depending on the @p autoIncrement parameter.
 *
 * @tparam autoIncrement When @c true (default), appends @c AUTOINCREMENT
 *                       to the DDL suffix. Set to @c false for tables
 *                       that require a plain primary key without
 *                       auto-increment behaviour.
 *
 * @note @c AUTOINCREMENT is a SQLite-specific keyword. Omitting it still
 *       guarantees uniqueness, but does not prevent reuse of deleted rowids.
 */
template <bool autoIncrement = true>
struct PrimaryKey
{
    /// Whether `AUTOINCREMENT` is appended to the DDL clause.
    static constexpr bool AutoIncrement = autoIncrement;

    /// The DDL suffix appended to the column definition.
    static constexpr std::string_view Suffix =
        AutoIncrement ? " PRIMARY KEY AUTOINCREMENT" : " PRIMARY KEY";
};

/**
 * @brief Marks the column as `NOT NULL`.
 *
 * Appends `NOT NULL` to the column's DDL definition, preventing the
 * storage of @c NULL values in that column.
 */
struct NotNull
{
    /// The DDL suffix appended to the column definition.
    static constexpr std::string_view Suffix = " NOT NULL";
};

/**
 * @brief Marks the column as @c UNIQUE.
 *
 * Appends `UNIQUE` to the column's DDL definition, enforcing that all
 * values in the column are distinct across rows.
 */
struct Unique
{
    /// The DDL suffix appended to the column definition.
    static constexpr std::string_view Suffix = " UNIQUE";
};

/**
 * @brief Attaches a raw SQL @c CHECK constraint to the column.
 *
 * Generates a @c CHECK(<Expr>) clause in the column's DDL definition,
 * where @c <Expr> is the compile-time string supplied as @p expr.
 *
 * @tparam expr The SQL boolean expression to embed in the @c CHECK clause.
 *              Must be a valid SQL expression referencing the column.
 *
 * @par Example
 * @code
 * // Produces: CHECK(age >= 0)
 * using AgeCheck = Check<"age >= 0">;
 * @endcode
 */
template <FixedString expr>
struct Check
{
    /// Stores the @c FixedString value to extend its lifetime.
    static constexpr auto expr_storage           = expr;

    /// The SQL expression embedded inside the @c CHECK(...) clause.
    static constexpr std::string_view Expression = expr_storage.view();
};

/**
 * @brief Attaches a regex-based @c CHECK constraint to the column.
 *
 * Generates a `CHECK(regexp('<Pattern>', column_name))` clause, relying
 * on a user-registered @c regexp SQL function (e.g. via @c sqlite3_create_function).
 *
 * @tparam expr The regex pattern string to match against column values.
 *
 * @par Example
 * @code
 * // Produces: CHECK(regexp('^[A-Z]{2}[0-9]+$', column_name))
 * using CodePattern = Regex<"^[A-Z]{2}[0-9]+$">;
 * @endcode
 *
 * @warning This constraint has no effect unless a @c regexp function is
 *          registered with the SQLite connection at runtime.
 */
template <FixedString expr>
struct Regex
{
    /// Stores the @c FixedString value to extend its lifetime.
    static constexpr auto expr_storage        = expr;

    /// The regex pattern passed as the first argument to @c regexp(...).
    static constexpr std::string_view Pattern = expr_storage.view();
};

/**
 * @brief Declares a @c REFERENCES foreign key constraint.
 *
 * Generates a `REFERENCES <Table>(<pk>)` clause in the column's DDL
 * definition, where the target column is identified by the member pointer
 * @p ReferencedMember within @p ReferencedTable.
 *
 * @tparam ReferencedTable  The C++ struct/class representing the referenced table.
 * @tparam ReferencedMember A member pointer (e.g. @c &OtherTable::id) identifying
 *                          the specific column in the referenced table.
 *
 * @par Example
 * @code
 * struct User { int id; std::string name; };
 *
 * // Column references User.id
 * using UserFK = ForeignKey<UserTable, &User::id>;
 * @endcode
 */
template <typename ReferencedTable, auto ReferencedMember>
struct ForeignKey
{
    /// The referenced table type.
    using Table = ReferencedTable;

    /// Member pointer identifying the referenced column within @c Table.
    static constexpr auto Member = ReferencedMember;
};
/// @}
} // namespace tewi
