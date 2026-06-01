export module tewi:member_traits;

import std;
// ============================================================================
//  MemberPtrTraits - decompose pointer-to-member types
// ============================================================================

export namespace tewi::detail
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
    using FieldType  = Field;
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
    sizeof...(MPs) > 0 &&
    (std::is_same_v<ObjectOf<MPs>,
                    ObjectOf<firstOf<MPs...>>> && ...);

// All member pointers must belong to the same class T.
template <auto... MPs>
requires HomogeneousMemberPtrs<MPs...>
using projection_object_t = ObjectOf<firstOf<MPs...>>;

// Requires that the two columns have member pointers that can
// be compared for equality (i.e. they point to the same field).
template<typename A, typename B>
concept ComparableMemberPtr =
requires
{
    { A::MemberPtr == B::MemberPtr } -> std::convertible_to<bool>;
};
} // namespace tewi::detail
