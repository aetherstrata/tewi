// ============================================================================
// SelectSpec.ixx  –  tewi:select_spec
//
// A typed, immutable AST for a SELECT query.  Nothing in this file emits SQL.
// SqliteRenderer (select_renderer.ixx) is the only place that does.
// ============================================================================
module tewi:ast_spec;

import :order;
import :compare;
import :sqlite_statement;

import std;

namespace tewi::ast
{
// -----------------------------------------------------------------------
// Predicate nodes
// -----------------------------------------------------------------------

/// A single bound WHERE predicate: column OP ?.
/// The binder captures the value; the renderer emits the placeholder.
struct PredicateNode
{
    std::string column; ///< Fully qualified: "table.col" or "col"
    Compare op;         ///< "=", "<", "LIKE", ...
};

// -----------------------------------------------------------------------
// AssignmentNode – one "col = ?" pair used by INSERT and UPDATE
// -----------------------------------------------------------------------

/// Represents a single column assignment: "col = ?".
/// Used in InsertSpec (VALUES list) and UpdateSpec (SET clause).
struct AssignmentNode
{
    std::string column;  ///< Plain column name (not qualified)
};

// -----------------------------------------------------------------------
// InsertSpec – INSERT [OR REPLACE] INTO <table> (<cols>) VALUES (?, ...)
// -----------------------------------------------------------------------

struct InsertSpec
{
    std::string_view table;
    bool or_replace = false;                 ///< OR REPLACE conflict clause
    std::vector<AssignmentNode> assignments; ///< one entry per column
};

// -----------------------------------------------------------------------
// UpdateSpec – UPDATE <table> SET col=?, ... WHERE col=?, ...
// -----------------------------------------------------------------------

struct UpdateSpec
{
    std::string_view            table;
    std::vector<AssignmentNode> assignments; ///< SET clause (non-PK columns)
    std::vector<PredicateNode>  where;       ///< WHERE clause (PK columns)
};

// -----------------------------------------------------------------------
// DeleteSpec – DELETE FROM <table> WHERE col=?, ...
// -----------------------------------------------------------------------

struct DeleteSpec
{
    std::string_view           table;
    std::vector<PredicateNode> where;   ///< WHERE predicates (PK columns)
};

// -----------------------------------------------------------------------
// Projection
// -----------------------------------------------------------------------

enum class ProjectionKind { AllColumns, ColumnList, CountStar };

struct ProjectionNode
{
    ProjectionKind kind = ProjectionKind::AllColumns;
    bool distinct       = false;
    std::vector<std::string> columns; ///< empty <--> AllColumns
    /// Optional single-column DISTINCT: "SELECT DISTINCT col FROM ..."
    std::optional<std::string> distinct_col;
};

// -----------------------------------------------------------------------
// FROM source
// -----------------------------------------------------------------------

struct TableRef
{
    std::string_view name;       ///< compile-time table name
    std::string all_columns_sql; ///< pre-built "t.c1, t.c2, ..."
};

// -----------------------------------------------------------------------
// JOIN clause
// -----------------------------------------------------------------------

enum class JoinKind { Inner, Left, Right, Full };

struct JoinNode
{
    JoinKind kind = JoinKind::Inner;
    TableRef table;
    std::string left_qual;  ///< "left_table.col"
    std::string right_qual; ///< "right_table.col"
};

// -----------------------------------------------------------------------
// ORDER BY
// -----------------------------------------------------------------------

struct OrderNode
{
    std::string column;
    Order direction = Order::ASC;
};

// -----------------------------------------------------------------------
// SelectSpec – the full immutable query description
// -----------------------------------------------------------------------

/// Immutable description of a SELECT statement.
/// Constructed incrementally by BasicQuery; consumed by SqliteRenderer.
struct SelectSpec
{
    ProjectionNode projection;
    TableRef from;
    std::vector<JoinNode> joins;
    std::vector<PredicateNode> where;
    std::vector<OrderNode> order_by;
    std::optional<i32> limit;
    std::optional<i32> offset;
};
} // namespace tewi::ast
