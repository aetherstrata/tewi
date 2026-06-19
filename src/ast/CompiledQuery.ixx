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
    [[nodiscard]] std::string str() const { return sql.str(); }

    void operator<<(std::string_view s) { sql << s; }

private:
    std::ostringstream sql;
};

/// A sequence of parameter binders, produced fresh for each execution.
/// Decoupled from the SQL string so the string can be compiled once
/// and the params rebuilt cheaply per row/invocation.
struct BoundParams
{
    using Binder = std::move_only_function<void(engine::SqliteStatement&, i32) const>;

    std::vector<Binder> binders;

    void bind(engine::SqliteStatement& stmt) const
    {
        i32 idx = 1;
        for (const auto& b : binders) b(stmt, idx++);
    }

    void push(Binder b) { binders.emplace_back(std::move(b)); }
};
} // namespace tewi::ast
