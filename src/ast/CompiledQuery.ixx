module tewi:ast_compiled;

import std;
import :sqlite_statement;

// -----------------------------------------------------------------------
// CompiledQuery – output of the renderer: ready SQL + bound parameters
// -----------------------------------------------------------------------

namespace tewi::ast
{

/// The immutable SQL string for a query shape.
/// Contains no runtime values — safe to cache and reuse indefinitely.
struct CompiledShape
{
    CompiledShape(std::string s) : sqlString(std::move(s)) {}

    [[nodiscard]] const std::string& sql() const { return sqlString; }

private:
    std::string sqlString;
};

/// A sequence of parameter binders, produced fresh for each execution.
/// Decoupled from the SQL string so the string can be compiled once
/// and the params rebuilt cheaply per row/invocation.
struct BoundParams
{
    using Binder = std::move_only_function<void(engine::SqliteStatement&) const>;

    std::unordered_map<std::string, Binder> binders;

    void bind(engine::SqliteStatement& stmt) const
    {
        for (const auto& b : binders | std::views::values) b(stmt);
    }

    void push(std::string_view name, Binder b) { binders.emplace(std::string(name), std::move(b)); }
};
} // namespace tewi::ast
