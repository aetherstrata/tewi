export module tewi:column;

import :ast_spec;
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

    [[nodiscard]] static constexpr ast::ColumnDefNode column_def()
    {
        ast::ColumnDefNode node{
            .name          = std::string(columnName),
            .type_affinity = std::string(SqliteTypeAdapter<FieldType>::affinity)
        };

        ([&]() constexpr
        {
            using C = Cs;
            if constexpr (std::is_same_v<C, NotNull>)
            {
                node.constraints.emplace_back(ast::NotNullConstraintNode{});
            }
            else if constexpr (std::is_same_v<C, Unique>)
            {
                node.constraints.emplace_back(ast::UniqueConstraintNode{});
            }
            else if constexpr (requires { C::expression; })
            {
                node.constraints.emplace_back(ast::CheckConstraintNode{std::string(C::expression)});
            }
            else if constexpr (requires { C::pattern; })
            {
                node.constraints.emplace_back(ast::RegexConstraintNode{std::string(C::pattern)});
            }
            else if constexpr (detail::is_foreign_key<C>::value)
            {
                using RT = C::Table;
                node.constraints.emplace_back(ast::ForeignKeyConstraintNode{
                    std::string(RT::tableName),
                    std::string(RT::template ColumnOf<C::member>::columnName)
                });
            }
        }(), ...);

        return node;
    }
};

/// Tag to delimit columns from indexes in the @c Table parameter list.
template <typename... Is>
struct Columns
{};
} // namespace tewi
