// ============================================================================
// Repository.ixx  –  tewi:repository
//
// Pure CRUD façade.  All read-side logic is delegated to BasicQuery via
// the query() entry point.  countWhere() uses SqliteRenderer directly
// to compile a COUNT(*) from the query's current SelectSpec.
// ============================================================================
export module tewi:repository;

import :basic_query;
import :row_hydrator;
import :ast_spec;
import :select_renderer;
import :sqlite_connection;
import :sqlite_statement;
import :sqlite_transaction;
import :table;
import :type_adapter;
import :base_adapters;

import std;

export namespace tewi
{
template <ITable TableType>
class Repository
{
    using RowType = TableType::RowType;
    using KeyType = TableType::KeyType;
public:

    explicit Repository(engine::SqliteConnection& db) noexcept
        : _db(db)
    {}

    // ----------------------------------------------------------------
    // query() – fluent BasicQuery root (read side)
    // ----------------------------------------------------------------
    [[nodiscard]] auto query() const { return detail::make_basic_query<TableType>(_db); }

    // ----------------------------------------------------------------
    // all() – fetch every row
    // ----------------------------------------------------------------
    [[nodiscard]] std::vector<RowType> all() const { return query().toVector(); }

    // ----------------------------------------------------------------
    // findBy<MemberPtr>(value) – find first matching row
    // ----------------------------------------------------------------
    template <auto MemberPtr, typename V>
    [[nodiscard]] std::optional<RowType> findBy(V&& value) const
    {
        return query().template where<MemberPtr>(std::forward<V>(value)).limit(1).firstOrDefault();
    }

    // ----------------------------------------------------------------
    // count() – total row count
    // ----------------------------------------------------------------
    [[nodiscard]] i64 count() const
    {
        // Build a minimal SelectSpec and render COUNT(*)
        ast::SelectSpec spec;
        spec.from       = detail::make_table_ref<TableType>();
        spec.projection = {.kind = ast::ProjectionKind::CountStar};

        auto cq   = ast::compile(spec);
        auto stmt = _db.prepare(cq.str());
        // No WHERE bindings needed for a plain count
        if (!stmt.step()) return 0;
        return SqliteTypeAdapter<i64>::read(stmt, 0);
    }

    // ----------------------------------------------------------------
    // insert() – INSERT OR REPLACE
    // ----------------------------------------------------------------
    [[nodiscard]] i64 insert(const RowType& obj) const
    {
        const std::string sql = build_insert_sql();
        auto stmt             = _db.prepare(sql);
        TableType::bind_all(stmt, obj);
        stmt.exec();
        return _db.lastInsertRowId();
    }

    void insert(const std::span<RowType>& rows) const
    {
        if (rows.empty()) return;
        auto txn  = _db.beginTransaction();
        const std::string sql = build_insert_sql();
        auto stmt = _db.prepare(sql);
        for (const auto& row : rows)
        {
            stmt.reset();
            stmt.clearBindings();
            TableType::bind_all(stmt, row, 1);
            stmt.exec();
        }
        txn.commit();
    }

    // ----------------------------------------------------------------
    // update() – UPDATE by primary key
    // ----------------------------------------------------------------
    void update(const RowType& obj) const
    {
        const std::string sql = build_update_sql();
        auto stmt             = _db.prepare(sql);

        // Bind non-PK fields first, then PK for WHERE
        i32 idx = 1;
        std::apply([&](auto... col_tags)
        {
            ([&](auto col_tag)
            {
                using ColType = decltype(col_tag);
                if constexpr (!ColType::isPrimaryKey)
                {
                    using FT = typename ColType::FieldType;
                    SqliteTypeAdapter<FT>::bind(stmt, idx++, obj.*(ColType::member));
                }
            }(col_tags), ...);
        }, typename TableType::ColumnsTuple{});

        TableType::bind_primary_key(stmt, obj, idx);
        stmt.step();
    }

    // ----------------------------------------------------------------
    // erase() – DELETE by primary key
    // ----------------------------------------------------------------
    template <typename... PKValues>
    requires (sizeof...(PKValues) > 1) // Required otherwise this would be ambiguous with the single-key overload
    void erase(PKValues&&... pk_values) const
    {
        erase(std::make_tuple(std::forward<PKValues>(pk_values)...));
    }

    void erase(KeyType key) const
    {
        const std::string sql = "DELETE FROM " + std::string(TableType::tableName) + " WHERE "
                                + TableType::primary_key_where_clause() + ";";

        auto stmt = _db.prepare(sql);
        i32 idx = 1;
        TableType::bind_primary_key(stmt, key);
        stmt.step();
    }

    void erase(const RowType& obj) const
    {
        const std::string sql = "DELETE FROM " + std::string(TableType::tableName) + " WHERE "
                                + TableType::primary_key_where_clause() + ";";

        auto stmt = _db.prepare(sql);
        TableType::bind_primary_key(stmt, obj);
        stmt.step();
    }

private:
    engine::SqliteConnection& _db;

    // NOTE: DDL-style SQL (INSERT / UPDATE patterns) is deliberately kept
    // here and not in SqliteRenderer, because these are row-mutation
    // operations that don't share structure with SELECT queries.

    [[nodiscard]] static std::string build_insert_sql()
    {
        std::string cols, placeholders;
        bool first = true;
        std::apply([&](auto... col_tags)
        {
            ([&](auto col_tag)
            {
                if (!first)
                {
                    cols         += ", ";
                    placeholders += ", ";
                }
                cols         += std::string(decltype(col_tag)::columnName);
                placeholders += "?";
                first         = false;
            }(col_tags), ...);
        }, typename TableType::ColumnsTuple{});

        return "INSERT OR REPLACE INTO " + std::string(TableType::tableName) + " (" + cols
               + ") VALUES (" + placeholders + ");";
    }

    [[nodiscard]] static std::string build_update_sql()
    {
        std::string sets;
        bool first = true;
        std::apply([&](auto... col_tags)
        {
            ([&](auto col_tag)
            {
                using ColType = decltype(col_tag);
                if constexpr (!ColType::isPrimaryKey)
                {
                    if (!first) sets += ", ";
                    sets  += std::string(ColType::columnName) + " = ?";
                    first  = false;
                }
            }(col_tags), ...);
        }, typename TableType::ColumnsTuple{});

        return "UPDATE " + std::string(TableType::tableName) + " SET " + sets + " WHERE "
               + TableType::primary_key_where_clause() + ";";
    }
};
} // namespace tewi
