export module tewi:fixed_string;

import :number_types;

export namespace tewi
{
    // ============================================================================
    //  §1  FixedString  - compile-time string NTTP
    // ============================================================================

    /// @class FixedString
    ///
    /// @brief A compile-time string that can be passed as a non-type template parameter.
    template <usize N>
    struct FixedString
    {
        static constexpr usize size = N - 1; // null terminator excluded

        consteval FixedString(const char (&str)[N]) noexcept
        {
            for (usize i = 0; i < N; ++i)
            {
                data[i] = str[i];
            }
        }

        [[nodiscard]] constexpr std::string_view view() const noexcept { return std::string_view(data.data(), size); }

        [[nodiscard]] constexpr bool operator==(const FixedString&) const = default;

        std::array<char, N> data{};
    };

} // namespace qilin

