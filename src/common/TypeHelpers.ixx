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

/// @brief Trait that is @c true when @c T is any @c std::tuple specialisation.
template <typename T>
inline constexpr bool isTuple = false;

template <typename... Ts>
inline constexpr bool isTuple<std::tuple<Ts...>> = true;

/**
 * @brief Removes one layer of @c std::optional wrapping from @c T.
 *        If @c T is not an @c optional, the type is left unchanged.
 */
template <typename T>
struct remove_optional : std::type_identity<T>
{};

template <typename T>
struct remove_optional<std::optional<T>> : std::type_identity<T>
{};

template <typename T>
using RemoveOptional = remove_optional<T>::type;

} // namespace tewi::detail
