module tewi:string_map;

import std;
import :number_types;

namespace tewi
{
struct StringHash
{
    using is_transparent = void;

    [[nodiscard]] usize operator()(const char* txt) const
    {
        return std::hash<std::string_view>{}(txt);
    }
    [[nodiscard]] usize operator()(std::string_view txt) const
    {
        return std::hash<std::string_view>{}(txt);
    }
    [[nodiscard]] usize operator()(const std::string& txt) const
    {
        return std::hash<std::string>{}(txt);
    }
};
template <typename Value>
using StringMap = std::unordered_map<std::string, Value, StringHash, std::equal_to<>>;
} // namespace tewi
