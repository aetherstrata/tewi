module tewi;

namespace tewi::ast
{

// -----------------------------------------------------------------------
// Projection clause
// -----------------------------------------------------------------------
[[nodiscard]] static std::string projection_sql(const ProjectionNode& proj)
{
    switch (proj.kind)
    {
    case ProjectionKind::CountStar: return "COUNT(*)";

    case ProjectionKind::ColumnList: {
        std::string s = proj.distinct ? "DISTINCT " : "";
        // Single-column DISTINCT form: SELECT DISTINCT col FROM ...
        if (proj.distinct_col) return "DISTINCT " + *proj.distinct_col;

        bool first = true;
        for (const auto& col : proj.columns)
        {
            if (!first) s += ", ";
            s     += col;
            first  = false;
        }
        return s;
    }

    case ProjectionKind::AllColumns:
    default:
        return (proj.distinct ? "DISTINCT " : "")
               + std::string("*"); // replaced by caller with column_list
    }
}

// -----------------------------------------------------------------------
// WHERE clause - also fills CompiledShape._binders
// -----------------------------------------------------------------------
[[nodiscard]] static std::string where_sql(const std::vector<PredicateNode>& preds)
{
    std::string s = " WHERE ";
    bool first    = true;
    for (const auto& pred : preds)
    {
        if (!first) s += " AND ";
        s += pred.column + " " + std::string(toSql(pred.op)) + " " + pred.param_name;
        first = false;
    }
    return s;
}

// -----------------------------------------------------------------------
// ORDER BY clause
// -----------------------------------------------------------------------
[[nodiscard]] static std::string order_sql(const std::vector<OrderNode>& orders)
{
    std::string s = " ORDER BY ";
    bool first    = true;
    for (const auto& [col, dir] : orders)
    {
        if (!first) s += ", ";
        s     += col + " " + std::string(toSql(dir));
        first  = false;
    }
    return s;
}

// -----------------------------------------------------------------------
// JOIN clause
// -----------------------------------------------------------------------
[[nodiscard]] static std::string join_sql(const JoinNode& j)
{
    std::string s = " ";
    switch (j.kind)
    {
    case JoinKind::Left:  s += "LEFT "; break;
    case JoinKind::Right: s += "RIGHT "; break;
    case JoinKind::Full:  s += "FULL "; break;
    default:              break; // Inner: no prefix
    }
    s += "JOIN " + std::string(j.table.name);
    s += " ON " + j.left_qual + " = " + j.right_qual;
    return s;
}

[[nodiscard]] static std::string build_select(const SelectSpec& spec)
{
    std::ostringstream ss;

    ss << "SELECT ";
    ss << projection_sql(spec.projection);
    ss << " FROM ";
    ss << spec.from.name;

    for (const auto& join : spec.joins) ss << join_sql(join);

    if (!spec.where.empty()) ss << where_sql(spec.where);

    if (!spec.order_by.empty()) ss << order_sql(spec.order_by);

    if (spec.limit) ss << " LIMIT " + std::to_string(*spec.limit);

    if (spec.offset) ss << " OFFSET " + std::to_string(*spec.offset);

    ss << ";";

    return ss.str();
}

[[nodiscard]] CompiledShape compile(const SelectSpec& spec)
{
    return CompiledShape{build_select(spec)};
}

// -----------------------------------------------------------------------
// compile(InsertSpec) -> CompiledShape
//
// Emits: INSERT [OR REPLACE] INTO <table> (<c1>, <c2>, ...) VALUES (?, ?, ...)
// -----------------------------------------------------------------------
[[nodiscard]] CompiledShape compile(const InsertSpec& spec)
{
    std::ostringstream ss;

    ss << (spec.or_replace ? "INSERT OR REPLACE INTO " : "INSERT INTO ");
    ss << spec.table;
    ss << " (";

    for (std::size_t i = 0; i < spec.assignments.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << spec.assignments[i].column;
    }

    ss << ") VALUES (";

    for (std::size_t i = 0; i < spec.assignments.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << spec.assignments[i].param_name;
    }

    ss << ");";

    return CompiledShape{ss.str()};
}

// -----------------------------------------------------------------------
// compile(UpdateSpec) -> CompiledShape
//
// Emits: UPDATE <table> SET <c1>=?, <c2>=?, ... WHERE <pk1>=?, ...
// -----------------------------------------------------------------------
[[nodiscard]] CompiledShape compile(const UpdateSpec& spec)
{
    std::ostringstream ss;

    ss << "UPDATE ";
    ss << spec.table;
    ss << " SET ";

    for (std::size_t i = 0; i < spec.assignments.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << spec.assignments[i].column;
        ss << " = " << spec.assignments[i].param_name;
    }

    if (!spec.where.empty()) {
        ss << " WHERE ";
        for (std::size_t i = 0; i < spec.where.size(); ++i) {
            if (i > 0) ss << " AND ";
            ss << spec.where[i].column;
            ss << " " << toSql(spec.where[i].op);   // existing helper used by SELECT compile()
            ss << " " << spec.where[i].param_name;
        }
    }

    ss << ";";
    return CompiledShape{ss.str()};
}

// -----------------------------------------------------------------------
// compile(DeleteSpec) -> CompiledShape
//
// Emits: DELETE FROM <table> WHERE <pk1>=?, ...
// -----------------------------------------------------------------------
[[nodiscard]] CompiledShape compile(const DeleteSpec& spec)
{
    std::ostringstream ss;

    ss << "DELETE FROM ";
    ss << spec.table;

    if (!spec.where.empty()) {
        ss << " WHERE ";
        for (std::size_t i = 0; i < spec.where.size(); ++i) {
            if (i > 0) ss << " AND ";
            ss << spec.where[i].column;
            ss << " " << toSql(spec.where[i].op);
            ss << " " << spec.where[i].param_name;
        }
    }

    ss << ";";
    return CompiledShape{ss.str()};
}

} // namespace tewi::ast
