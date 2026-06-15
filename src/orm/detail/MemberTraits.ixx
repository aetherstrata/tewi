module tewi:member_traits;

import std;
// ============================================================================
//  MemberPtrTraits - decompose pointer-to-member types
// ============================================================================

namespace tewi::detail
{
/**
 * @brief Extracts the first value from a non-type template parameter pack.
 *
 * Useful when @c std::get<0> is unavailable because the pack is a value pack
 * (auto...) rather than a type pack.
 * @tparam First The first value in the pack.
 * @tparam Rest  Remaining values (ignored).
 */
template <auto First, auto... Rest>
struct first_of
{
    /// Stores the first value of the pack.
    static constexpr auto value = First;
};

/**
 * @brief Convenience variable template for @c first_of to directly access the first value.
 * @tparam Vs Pack of values to extract from. The first value is returned as a constexpr.
 */
template <auto... Vs>
constexpr auto firstOf = first_of<Vs...>::value;

/**
 * @brief Checks if @b any of the provided boolean template parameters are @c true.
 * @tparam T Pack of boolean values to evaluate.
 * @note Equivalent to a fold expression `(... || T)`.
 */
template <bool... T>
constexpr bool anyOf = (... || T);

/**
 * @brief Checks if @b all the provided boolean template parameters are @b true.
 * @tparam T Pack of boolean values to evaluate.
 * @note Equivalent to a fold expression `(... && T)`.
 */
template <bool... T>
constexpr bool allOf = (... && T);

/**
 * @brief Primary template for member pointer decomposition.
 *
 * This template is intentionally left undefined. Only the partial
 * specialization for member pointer types is valid. Instantiating
 * this primary template with a non-member-pointer type will result
 * in a compile-time error.
 *
 * @tparam T A member pointer type of the form `Field Obj::*`.
 *
 * @see member_ptr<Field Obj::*>
 */
template <typename T>
struct member_ptr;

/**
 * @brief Partial specialization that decomposes a member pointer type.
 *
 * Given a member pointer type `Field Obj::*`, this trait exposes the
 * enclosing class type and the field type as nested type aliases.
 *
 * @tparam Obj   The class type that contains the member.
 * @tparam Field The type of the member pointed to.
 *
 * @par Example
 * @code
 * struct Foo { i32 x; };
 *
 * using MP = member_ptr<decltype(&Foo::x)>;
 * static_assert(std::is_same_v<MP::ObjectType, Foo>);
 * static_assert(std::is_same_v<MP::FieldType,  i32>);
 * @endcode
 */
template <typename Obj, typename Field>
struct member_ptr<Field Obj::*>
{
    /// The class type that owns the member (`Obj` in `Field Obj::*`).
    using ObjectType = Obj;

    /// The type of the member being pointed to (`Field` in `Field Obj::*`).
    using FieldType = Field;
};

template <auto MP, typename... Ts>
struct mp_column;

template <auto MP>
struct mp_column<MP>
{
    using type = void;
};

template <auto MP, typename Col, typename... Rest>
struct mp_column<MP, Col, Rest...>
{
    static consteval bool match()
    {
        if constexpr (requires { Col::member == MP; })
        {
            return Col::member == MP;
        }
        else
            return false;
    }

    using type = std::conditional_t<match(), Col, typename mp_column<MP, Rest...>::type>;
};

// MemberPtrs... -> std::tuple<FieldType1, FieldType2, …>
template <auto... MemberPtrs>
using MembersTuple =
    std::tuple<typename member_ptr<std::remove_cv_t<decltype(MemberPtrs)>>::FieldType...>;

// Extract the object type from a single member pointer type.
template <auto MP>
using ObjectOf = member_ptr<std::remove_cv_t<decltype(MP)>>::ObjectType;

// All MPs share the same object type - works for 1 or more MPs.
template <auto... MPs>
concept HomogeneousMemberPtrs =
    sizeof...(MPs) > 0 && (std::is_same_v<ObjectOf<MPs>, ObjectOf<firstOf<MPs...>>> && ...);

template <typename First, typename... Rest>
consteval bool unique_member_ptrs()
{
    if constexpr (sizeof...(Rest) == 0)
    {
        return true;
    }
    else
    {
        return ((
                    []<typename Other>()
        {
            if constexpr (requires { First::member == Other::member; })
            {
                return First::member != Other::member;
            }
            else
            {
                return true;
            }
        }.template operator()<Rest>())
                && ...)
               && unique_member_ptrs<Rest...>();
    }
}

template <typename... Cols>
concept UniqueMemberPointers = unique_member_ptrs<Cols...>();

template <auto MP>
using FieldOf = member_ptr<decltype(MP)>::FieldType;

template <auto... MP>
using FieldsOf = std::tuple<FieldOf<MP>...>;

template <auto... MemberPtrs>
struct projection_result;

template <auto MP>
struct projection_result<MP>
{
    using type = FieldOf<MP>;
};

template <auto MP1, auto MP2, auto... Rest>
struct projection_result<MP1, MP2, Rest...>
{
    using type = std::tuple<FieldOf<MP1>, FieldOf<MP2>, FieldOf<Rest>...>;
};

template <auto... MemberPtrs>
using ProjectionResult = typename projection_result<MemberPtrs...>::type;

template <typename Tuple, typename... Ts>
concept SameAsTuple = []<typename... Us>(std::tuple<Us...>*)
{
    return sizeof...(Ts) == sizeof...(Us) && (std::is_same_v<Ts, Us> && ...);
}(static_cast<Tuple*>(nullptr));

template <typename T>
inline constexpr bool isTuple = false;

template <typename... Ts>
inline constexpr bool isTuple<std::tuple<Ts...>> = true;

template <typename T>
struct remove_optional : std::type_identity<T>
{};

template <typename T>
struct remove_optional<std::optional<T>> : std::type_identity<T>
{};

template <typename T>
using RemoveOptional = remove_optional<T>::type;
} // namespace tewi::detail
