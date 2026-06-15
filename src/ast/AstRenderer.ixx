// ============================================================================
// SqliteRenderer.ixx  -  tewi:select_renderer
//
// The single place that turns a SelectSpec into a SQL string + bindings.
// No query class, no Table, no Column ever calls string concatenation again.
// ============================================================================
module tewi:select_renderer;

import :ast_spec;
import :compare;
import :order;

import std;

namespace tewi::ast
{
[[nodiscard]] CompiledQuery compile(const SelectSpec& spec);

/// Convenience: compile a spec and return just the SQL string (for tests/logging).
[[nodiscard]] inline std::string to_sql(const SelectSpec& spec)
{
    return compile(spec).str();
}
} // namespace tewi::ast
