export module tewi:compare;

import std;

namespace tewi
{
export enum class Compare { Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual };

constexpr std::string_view toSql(Compare cmp)
{
    switch (cmp)
    {
    case Compare::Equal:        return "=";
    case Compare::NotEqual:     return "!=";
    case Compare::Less:         return "<";
    case Compare::LessEqual:    return "<=";
    case Compare::Greater:      return ">";
    case Compare::GreaterEqual: return ">=";
    default:                    return "";
    }
}
} // namespace tewi
