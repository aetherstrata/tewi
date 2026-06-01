export module tewi:column;

import :constraint_helpers;
import :fk_helpers;
import :type_adapter;

import std;

// ============================================================================
//  Table Columns
// ============================================================================

export namespace tewi
{
/// Describes a single table column.
/// @tparam name    Compile-time column name string.
/// @tparam member  Pointer-to-member of the mapped C++ field.
/// @tparam Cs      Zero or more constraint tags.
template <FixedString name, auto member, typename... Cs>
struct Column
{
private:
    static constexpr auto name_storage           = name;
    using mp_traits   = detail::member_ptr<decltype(member)>;
public:
    using ObjectType  = mp_traits::ObjectType;
    using FieldType   = mp_traits::FieldType;
    using Constraints = std::tuple<Cs...>;

    static constexpr auto MemberPtr = member;

    static constexpr std::string_view ColumnName = name_storage.view();

    static constexpr bool IsPrimaryKey = detail::anyOf<detail::isPrimaryKey<Cs>...>;

    static constexpr bool IsAutoincrement = detail::anyOf<detail::isAutoincrementPK<Cs>...>;

    static constexpr bool HasForeignKey = detail::anyOf<detail::isForeignKey<Cs>...>;

    // ------------------------------------------------------------------
    // DDL fragment for this column (without trailing comma).
    // ------------------------------------------------------------------
    [[nodiscard]] static std::string ddl()
    {
        std::string sql = std::string(ColumnName) + " "
                          + std::string(SqliteTypeAdapter<FieldType>::affinity)
                          + detail::constraint_ddl<Cs...>(ColumnName);

        // Append REFERENCES clause for each ForeignKey constraint.
        ([&]()
        {
            if constexpr (detail::is_foreign_key<Cs>::value)
            {
                using RT  = Cs::Table;
                sql      += " REFERENCES " + std::string(RT::TableName) + "("
                       + std::string(RT::template column_name_for<Cs::Member>()) + ")";
            }
        }(), ...);
        return sql;
    }
};

/// Tag to delimit columns from indexes in the @c Table parameter list.
template <typename... Is>
struct Columns
{};

} // namespace tewi
