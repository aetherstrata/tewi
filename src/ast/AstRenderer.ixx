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
// SELECT / INSERT / UPDATE / DELETE specs carry runtime bound-parameter
// values, so they can never be rendered at compile time. Declared here,
// defined (using std::ostringstream) in AstRenderer.cppm.
[[nodiscard]] CompiledShape compile(const SelectSpec& spec);
[[nodiscard]] CompiledShape compile(const InsertSpec& spec);
[[nodiscard]] CompiledShape compile(const DeleteSpec& spec);
[[nodiscard]] CompiledShape compile(const UpdateSpec& spec);

// -----------------------------------------------------------------------
// DDL rendering (CreateTableSpec / CreateIndexSpec)
//
// Unlike the specs above, DDL carries no runtime bound values, so it can be
// rendered entirely at compile time. These are `constexpr` (not `consteval`)
// and fully defined *here*, in the module interface partition, rather than
// in AstRenderer.cppm - a definition sitting only in the .cppm implementation
// unit is not reachable for constant evaluation from other partitions
// (Column/Index/Table), since only interface partitions contribute their
// function bodies to the module's constexpr-evaluation surface.
//
// Building the AST and rendering it to a std::string both still use
// std::string/std::vector - fine for compile-time evaluation as long as
// neither escapes as the result of a `consteval` call (see Table::
// create_table_sql_fixed / Index::create_index_sql_fixed, which build the
// spec, render it here, then convert the resulting `std::string` into a
// `FixedString<N>` before returning - only that final literal type is
// allowed to escape the mandatory compile-time evaluation).
// -----------------------------------------------------------------------

[[nodiscard]] std::string constraint_sql(const ColumnConstraintNode& constraint, std::string_view col_name)
{
    return std::visit([&]<typename T>(const T& node) -> std::string
    {
        if constexpr (std::is_same_v<T, NotNullConstraintNode>)
        {
            return " NOT NULL";
        }
        else if constexpr (std::is_same_v<T, UniqueConstraintNode>)
        {
            return " UNIQUE";
        }
        else if constexpr (std::is_same_v<T, CheckConstraintNode>)
        {
            return " CHECK(" + node.expression + ")";
        }
        else if constexpr (std::is_same_v<T, RegexConstraintNode>)
        {
            return " CHECK(regexp('" + node.pattern + "', " + std::string(col_name) + "))";
        }
        else if constexpr (std::is_same_v<T, ForeignKeyConstraintNode>)
        {
            return " REFERENCES " + node.ref_table + "(" + node.ref_column + ")";
        }
        else static_assert("Unknown constraint type");
    }, constraint);
}

[[nodiscard]] std::string column_def_sql(const ColumnDefNode& col)
{
    std::string sql = col.name + " " + col.type_affinity;
    for (const auto& constraint : col.constraints) sql += constraint_sql(constraint, col.name);
    return sql;
}

// Emits: CREATE TABLE [IF NOT EXISTS] <table> (<col_defs>, PRIMARY KEY (<cols>))
[[nodiscard]] std::string render(const TableDefNode& spec)
{
    std::string sql = "CREATE TABLE ";
    if (spec.if_not_exists) sql += "IF NOT EXISTS ";
    sql += spec.table;
    sql += " (";

    for (std::size_t i = 0; i < spec.columns.size(); ++i)
    {
        if (i > 0) sql += ", ";
        sql += column_def_sql(spec.columns[i]);
    }

    sql += ", PRIMARY KEY (";
    bool first = true;
    for (const TableConstraintNode& constraint : spec.constraints)
    {
        sql += std::visit([&]<typename T>(const T& node)
        {
            if constexpr (std::is_same_v<T, PrimaryKeyConstraintNode>)
            {
                std::string part;
                if (!first) part += ", ";
                part += node.column_name;
                if (node.autoincrement) part += " AUTOINCREMENT";
                first = false;
                return part;
            }
            else return "";
        }, constraint);
    }
    sql += ")";

    sql += ");";

    return sql;
}

// Emits: CREATE [UNIQUE] INDEX [IF NOT EXISTS] <name> ON <table> (<cols>)
[[nodiscard]] std::string render(const IndexDefNode& spec)
{
    std::string sql = spec.unique ? "CREATE UNIQUE INDEX " : "CREATE INDEX ";
    if (spec.if_not_exists) sql += "IF NOT EXISTS ";
    sql += spec.index_name;
    sql += " ON ";
    sql += spec.table;
    sql += " (";

    for (std::size_t i = 0; i < spec.columns.size(); ++i)
    {
        if (i > 0) sql += ", ";
        sql += spec.columns[i];
    }

    sql += ");";
    return sql;
}
} // namespace tewi::ast
