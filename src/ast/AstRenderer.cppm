module tewi;

namespace tewi::ast
{
static std::string op_to_sql(Compare op)
{
    using tewi::Compare;
    switch (op)
    {
    case Compare::Equal:        return "=";
    case Compare::NotEqual:     return "!=";
    case Compare::Less:         return "<";
    case Compare::LessEqual:    return "<=";
    case Compare::Greater:      return ">";
    case Compare::GreaterEqual: return ">=";
    default:                    throw std::runtime_error("Invalid Compare operator");
    }
}
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
        // Single-column DISTINCT form: SELECT DISTINCT col FROM …
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
// WHERE clause - also fills CompiledQuery._binders
// -----------------------------------------------------------------------
[[nodiscard]] static std::string where_sql(const std::vector<PredicateNode>& preds,
                                           CompiledQuery& cq)
{
    std::string s = " WHERE ";
    bool first    = true;
    for (const auto& pred : preds)
    {
        if (!first) s += " AND ";
        s += pred.column + " " + op_to_sql(pred.op) + " ?";
        // Store a copy of the binder - pred is owned by SelectSpec
        cq << [&binder = pred.binder](engine::SqliteStatement& stmt, i32 idx)
        {
            binder(stmt, idx);
        };
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
        s     += col + (dir == Order::DESC ? " DESC" : " ASC");
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

void build_select(const SelectSpec& spec, CompiledQuery& cq)
{
    cq << "SELECT ";
    cq << projection_sql(spec.projection);
    cq << " FROM ";
    cq << spec.from.name;

    for (const auto& join : spec.joins) cq << join_sql(join);

    if (!spec.where.empty()) cq << where_sql(spec.where, cq);

    if (!spec.order_by.empty()) cq << order_sql(spec.order_by);

    if (spec.limit) cq << " LIMIT " + std::to_string(*spec.limit);

    if (spec.offset) cq << " OFFSET " + std::to_string(*spec.offset);

    cq << ";";
}

[[nodiscard]] CompiledQuery compile(const SelectSpec& spec)
{
    CompiledQuery cq;
    build_select(spec, cq);
    return cq;
}
} // namespace tewi::ast
