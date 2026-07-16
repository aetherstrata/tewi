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
    std::string param_name; ///< Bind parameter name including its SQLite sigil (e.g. ":p1")
};

// -----------------------------------------------------------------------
// AssignmentNode – one "col = ?" pair used by INSERT and UPDATE
// -----------------------------------------------------------------------

/// Represents a single column assignment: "col = ?".
/// Used in InsertSpec (VALUES list) and UpdateSpec (SET clause).
struct AssignmentNode
{
    std::string column;  ///< Plain column name (not qualified)
    std::string param_name; ///< Bind parameter name including its SQLite sigil (e.g. ":p1")
};

// -----------------------------------------------------------------------
// InsertSpec – INSERT [OR REPLACE] INTO <table> (<cols>) VALUES (?, ...)
// -----------------------------------------------------------------------

struct InsertSpec
{
    std::string  table;
    bool or_replace = false;                 ///< OR REPLACE conflict clause
    std::vector<AssignmentNode> assignments; ///< one entry per column
};

// -----------------------------------------------------------------------
// UpdateSpec – UPDATE <table> SET col=?, ... WHERE col=?, ...
// -----------------------------------------------------------------------

struct UpdateSpec
{
    std::string            table;
    std::vector<AssignmentNode> assignments; ///< SET clause (non-PK columns)
    std::vector<PredicateNode>  where;       ///< WHERE clause (PK columns)
};

// -----------------------------------------------------------------------
// DeleteSpec – DELETE FROM <table> WHERE col=?, ...
// -----------------------------------------------------------------------

struct DeleteSpec
{
    std::string           table;
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
    std::string name;       ///< compile-time table name
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
// Table-level constraint nodes (DDL) - one node type per constraint kind.
// -----------------------------------------------------------------------

struct PrimaryKeyConstraintNode
{
    std::string column_name;
    bool autoincrement = false;
};

using TableConstraintNode = std::variant<
    PrimaryKeyConstraintNode
>;

// -----------------------------------------------------------------------
// Column-level constraint nodes (DDL) - one node type per constraint kind.
// -----------------------------------------------------------------------

struct NotNullConstraintNode {};

struct UniqueConstraintNode {};

struct CheckConstraintNode
{
    std::string expression; ///< raw boolean expression, e.g. "age >= 0"
};

struct RegexConstraintNode
{
    std::string pattern; ///< regex pattern; rendered as CHECK(regexp('<pattern>', <col>))
};

struct ForeignKeyConstraintNode
{
    std::string ref_table;
    std::string ref_column;
};

using ColumnConstraintNode = std::variant<
    NotNullConstraintNode,
    UniqueConstraintNode,
    CheckConstraintNode,
    RegexConstraintNode,
    ForeignKeyConstraintNode
>;

// -----------------------------------------------------------------------
// ColumnDefNode – one column definition inside CREATE TABLE
// -----------------------------------------------------------------------

struct ColumnDefNode
{
    std::string name;
    std::string type_affinity;
    std::vector<ColumnConstraintNode> constraints;
};

// -----------------------------------------------------------------------
// CreateIndexSpec – CREATE [UNIQUE] INDEX [IF NOT EXISTS] <name> ON <table> (<cols>)
// -----------------------------------------------------------------------

struct IndexDefNode
{
    std::string index_name;
    std::string table;
    bool if_not_exists = true;
    bool unique = false;
    std::vector<std::string> columns;
};

// -----------------------------------------------------------------------
// CreateTableSpec – CREATE TABLE [IF NOT EXISTS] <table> (<cols>, PRIMARY KEY (...))
// -----------------------------------------------------------------------

struct TableDefNode
{
    std::string table;
    bool if_not_exists = true;
    std::vector<ColumnDefNode> columns;
    std::vector<IndexDefNode> indices;
    std::vector<TableConstraintNode> constraints;
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
