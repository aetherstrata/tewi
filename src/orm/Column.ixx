export module tewi:column;

import :fk_helpers;
import :pk_helpers;
import :type_adapter;

import std;

// ============================================================================
//  Table Columns
// ============================================================================

export namespace tewi
{
/// Describes a single table column.
/// @tparam name    Compile-time column name string.
/// @tparam MP  Pointer-to-member of the mapped C++ field.
/// @tparam Cs      Zero or more constraint tags.
template <FixedString name, auto MP, typename... Cs>
requires detail::ForeignKeyHasSameType<detail::FieldOf<MP>, Cs...>
    && std::is_default_constructible_v<detail::FieldOf<MP>>
struct Column
{
private:
    static constexpr auto name_storage           = name;
    using mp_traits   = detail::member_ptr<decltype(MP)>;
public:
    using ObjectType  = mp_traits::ObjectType;
    using FieldType   = mp_traits::FieldType;
    using Constraints = std::tuple<Cs...>;

    static constexpr auto member = MP;

    static constexpr std::string_view columnName = name_storage.view();

    static constexpr bool isPrimaryKey = detail::anyOf<detail::isPrimaryKey<Cs>...>;

    static constexpr bool isAutoincrement = detail::anyOf<detail::isAutoincrementPK<Cs>...>;

    static constexpr bool hasForeignKey = detail::anyOf<detail::isForeignKey<Cs>...>;

    // ------------------------------------------------------------------
    // DDL fragment for this column (without trailing comma).
    // ------------------------------------------------------------------
    [[nodiscard]] static std::string ddl()
    {
        std::string sql = std::string(columnName) + " "
                          + std::string(SqliteTypeAdapter<FieldType>::affinity)
                          + detail::constraint_ddl<Cs...>(columnName);

        // Append REFERENCES clause for each ForeignKey constraint.
        ([&]() constexpr
        {
            if constexpr (detail::is_foreign_key<Cs>::value)
            {
                using RT = Cs::Table;
                sql     += " REFERENCES " + std::string(RT::tableName) + "("
                    + std::string(RT::template ColumnOf<Cs::member>::columnName) + ")";
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
