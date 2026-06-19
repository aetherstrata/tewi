module tewi:type_helpers;

import std;

namespace tewi::detail
{

/**
 * @brief Extracts the first value from a non-type template parameter pack.
 *
 * Useful when @c std::get<0> is unavailable because the pack is a value pack
 * (@c auto...) rather than a type pack.
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
 * @note Equivalent to a fold expression <tt>(... || T)</tt>.
 */
template <bool... T>
constexpr bool anyOf = (... || T);

/**
 * @brief Checks if @b all the provided boolean template parameters are @c true.
 * @tparam T Pack of boolean values to evaluate.
 * @note Equivalent to a fold expression <tt>(... && T)</tt>.
 */
template <bool... T>
constexpr bool allOf = (... && T);

/**
 * @brief Checks whether @c T is a @c std::tuple specialisation with the same
 *        element types as @c std::tuple<Us...>.
 * @tparam T   Type to test.
 * @tparam Us  Expected element types.
 */
template <typename T, typename... Us>
concept SameAsTuple = [](std::tuple<Us...>*)
{
    if constexpr (requires { []<typename... Ts>(std::tuple<Ts...>*){} (static_cast<T*>(nullptr)); })
    {
        return []<typename... Ts>(std::tuple<Ts...>*)
        {
            return sizeof...(Ts) == sizeof...(Us) && (std::is_same_v<Ts, Us> && ...);
        }(static_cast<T*>(nullptr));
    }
    else return false;
}(static_cast<std::tuple<Us...>*>(nullptr));

/**
 * @brief Checks if a given type is a @c std::tuple
 *
 * This primary template yields @c false for any non-tuple type.
 *
 * @tparam T The type to check.
 */
template <typename T>
inline constexpr bool isTuple = false;

/**
 * @brief Specialization of @c isTuple for @c std::tuple
 *
 * @tparam Ts The types contained within the tuple.
 */
template <typename... Ts>
inline constexpr bool isTuple<std::tuple<Ts...>> = true;

/**
 * @brief Trait to prepend a type to the beginning of a tuple.
 *
 * @tparam T The type to prepend.
 * @tparam Tuple The target tuple type.
 */
template <typename T, typename Tuple>
struct tuple_prepend;

/**
 * @brief Specialization of @c tuple_prepend for @c std::tuple
 *
 * @tparam T The type to prepend.
 * @tparam Ts The existing types in the tuple.
 */
template <typename T, typename... Ts>
struct tuple_prepend<T, std::tuple<Ts...>>
{
    /** @brief The resulting tuple type with T placed at the front. */
    using type = std::tuple<T, Ts...>;
};

/**
 * @brief Extracts the underlying type from a @c std::optional
 *
 * If the provided type is not a @c std::optional, it acts as an identity trait
 * and yields the original type.
 *
 * @tparam T The type to inspect.
 */
template <typename T>
struct remove_optional : std::type_identity<T>
{};

/**
 * @brief Specialization of @c remove_optional for @c std::optional
 *
 * @tparam T The underlying type stored inside the optional wrapper.
 */
template <typename T>
struct remove_optional<std::optional<T>> : std::type_identity<T>
{};

/**
 * @brief Convenience alias for extracting a type from an optional wrapper.
 *
 * @tparam T The type to strip the optional wrapper from.
 */
template <typename T>
using RemoveOptional = remove_optional<T>::type;

} // namespace tewi::detail
