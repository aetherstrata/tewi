// ============================================================================
// SqliteRenderer.ixx  -  tewi:ast_renderer
//
// The single place that turns a Spec into a SQL string + bindings.
// ============================================================================
module tewi:ast_renderer;

import :ast_compiled;
import :ast_spec;
import :compare;
import :order;

import std;

namespace tewi::ast
{
[[nodiscard]] CompiledShape compile(const SelectSpec& spec);
[[nodiscard]] CompiledShape compile(const InsertSpec& spec);
[[nodiscard]] CompiledShape compile(const DeleteSpec& spec);
[[nodiscard]] CompiledShape compile(const UpdateSpec& spec);
} // namespace tewi::ast
