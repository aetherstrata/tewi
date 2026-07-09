module tewi:ast_compiled;

import :sqlite_statement;
import :string_map;
import :type_adapter;

import std;

// -----------------------------------------------------------------------
// CompiledQuery – output of the renderer: ready SQL + bound parameters
// -----------------------------------------------------------------------

namespace tewi::ast
{

/// Builds a SQLite named-parameter placeholder (e.g. "1" -> ":p1").
[[nodiscard]] inline std::string makeParamName(i32 n)
{
    return ":p" + std::to_string(n);
}

/// The immutable SQL string for a query shape.
/// Contains no runtime values - safe to cache and reuse indefinitely.
struct CompiledShape
{
    CompiledShape(std::string s) : sqlString(std::move(s)) {}

    [[nodiscard]] const std::string& sql() const { return sqlString; }

private:
    std::string sqlString;
};

struct BinderError : std::runtime_error
{
    BinderError(const std::string& s) : std::runtime_error(s) {}
};

/// A sequence of parameter binders, produced fresh for each execution.
/// Decoupled from the SQL string so the string can be compiled once
/// and the params rebuilt cheaply per row/invocation.
struct BoundParams
{
    using Binder = std::move_only_function<void(engine::SqliteStatement&) const>;

    StringMap<Binder> binders;

    void bind(engine::SqliteStatement& stmt) const
    {
        for (const auto& b : binders | std::views::values)
        {
            b(stmt);
        }
    }

    template <SqliteAdaptable T>
    void add(std::string_view name, T&& value)
    {
        if (binders.contains(name))
        {
            std::string msg = std::format("The parameter with name {} already has a bound value", name);
            throw BinderError(msg);
        }

        binders[std::string(name)] = [val = value, nm = std::string(name)] (engine::SqliteStatement& s)
        {
            using FT = std::remove_cvref_t<decltype(val)>;
            SqliteTypeAdapter<FT>::bind(s, nm, val);
        };
    }
};
} // namespace tewi::ast
