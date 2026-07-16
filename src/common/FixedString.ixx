module tewi:fixed_string;

import :number_types;

import std;

// ============================================================================
// FixedString - compile-time string NTTP
// ============================================================================

namespace tewi
{
/**
 * @struct FixedString
 * @brief Compile-time fixed string wrapper for non-type template parameters.
 *
 * This type stores a null-terminated character array at compile time and is
 * intended for use as a non-type template parameter (NTTP). The template
 * parameter @c N includes the terminating null character, while @ref size
 * excludes it.
 *
 * @tparam N Size of the underlying character array, including the null terminator.
 */
template <usize N>
struct FixedString
{
    /**
     * @brief Length of the string excluding the terminating null character.
     */
    static constexpr usize size = N - 1;

    /**
     * @brief Constructs a fixed string from a string literal.
     *
     * Copies all @c N characters, including the terminating null character,
     * into internal storage.
     *
     * @param str Null-terminated character array used to initialize the string.
     */
    consteval FixedString(const char (&str)[N]) noexcept
    {
        for (usize i = 0; i < N; ++i)
        {
            data[i] = str[i];
        }
    }

    /**
     * @brief Constructs a fixed string from a @c std::string_view.
     *
     * @pre @p sv.size() == N - 1. Lets a compile-time-rendered SQL string
     * (e.g. the result of rendering a DDL AST to a @c std::string within a
     * @c consteval function) be converted into a literal, allocation-free
     * value that can safely escape that compile-time evaluation.
     *
     * @param sv View whose contents are copied, followed by a null terminator.
     */
    consteval FixedString(std::string_view sv) noexcept
    {
        if (sv.length() != size)
            throw std::logic_error("string_view and FixedString must have the same length");

        for (usize i = 0; i < size; ++i)
        {
            data[i] = sv[i];
        }
        data[size] = '\0';
    }

    /**
     * @brief Returns the string contents as a string view.
     *
     * The returned view excludes the terminating null character.
     *
     * @return A @c std::string_view over the stored character sequence.
     */
    [[nodiscard]] constexpr std::string_view view() const noexcept { return {data.data(), size}; }

    /**
     * @brief Returns the string contents as a head-allocated @c std::string.
     *
     * The returned string excludes the terminating null character.
     *
     * @return A @c std::string over the stored character sequence.
     */
    [[nodiscard]] std::string str() const noexcept { return {data.data(), size}; }

    /**
     * @brief Compares two fixed strings for equality.
     *
     * Defaulted comparison checks the stored character array contents.
     *
     * @param other The string to compare against.
     * @return @c true if both strings contain identical characters, otherwise @c false.
     */
    [[nodiscard]] constexpr bool operator==(const FixedString& other) const = default;

    /**
     * @brief Storage for the string data, including the terminating null character.
     */
    std::array<char, N> data{};
};
} // namespace tewi

