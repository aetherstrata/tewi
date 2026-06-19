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
import :ast_compiled;
import :ast_renderer;
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
    // insert() – INSERT OR REPLACE (single row)
    // ----------------------------------------------------------------
    [[nodiscard]] i64 insert(const RowType& obj) const {
        auto cq   = ast::compile(build_insert_spec());
        auto stmt = _db.prepare(cq.str());
        build_insert_params(obj).bind(stmt);
        stmt.exec();
        return _db.lastInsertRowId();
    }

    // ----------------------------------------------------------------
    // insert() – INSERT OR REPLACE (batch)
    // ----------------------------------------------------------------
    void insert(const std::span<const RowType>& rows) const
    {
        if (rows.empty()) return;

        // Compile the SQL shape once — no values involved.
        const ast::CompiledShape shape = ast::compile(build_insert_spec());
        auto txn = _db.beginTransaction();

        for (const auto& row : rows) {
            // Produce only the bindings — cheap, no string work.
            ast::BoundParams params = build_insert_params(row);
            auto stmt = _db.prepare(shape.str());
            params.bind(stmt);
            stmt.exec();
        }
        txn.commit();
    }

    // ----------------------------------------------------------------
    // update() – UPDATE by primary key
    // ----------------------------------------------------------------
    void update(const RowType& obj) const {
        auto cq   = ast::compile(build_update_spec(obj));
        auto stmt = _db.prepare(cq.str());
        build_update_params(obj).bind(stmt);
        stmt.step();
    }

    // ----------------------------------------------------------------
    // erase() – DELETE by primary key (variadic composite key)
    // ----------------------------------------------------------------
    template <typename... PKValues>
    requires (sizeof...(PKValues) > 1)
    void erase(PKValues&&... pk_values) const {
        erase(std::make_tuple(std::forward<PKValues>(pk_values)...));
    }

    // ----------------------------------------------------------------
    // erase() – DELETE by primary key value (scalar or tuple)
    // ----------------------------------------------------------------
    void erase(KeyType key) const
    {
        auto cq   = ast::compile(build_delete_spec_from_key());
        auto stmt = _db.prepare(cq.str());
        build_delete_params(key).bind(stmt);
        stmt.step();
    }

    // ----------------------------------------------------------------
    // erase() – DELETE by row object (PK fields extracted)
    // ----------------------------------------------------------------
    void erase(const RowType& obj) const
    {
        auto cq   = ast::compile(build_delete_spec_from_row());
        auto stmt = _db.prepare(cq.str());
        build_delete_params(obj).bind(stmt);
        stmt.step();
    }

private:
    engine::SqliteConnection& _db;

    // ================================================================
    // DML AST spec builders
    //
    // Each helper produces a typed spec (InsertSpec / UpdateSpec /
    // DeleteSpec). ast::compile() in select_renderer is the ONLY site
    // that ever concatenates SQL strings.
    // ================================================================

    // ----------------------------------------------------------------
    // INSERT OR REPLACE INTO <table> (<col>, ...) VALUES (?, ...)
    // ----------------------------------------------------------------
    // Shape: column names only, no values
    [[nodiscard]] static ast::InsertSpec build_insert_spec()
    {
        ast::InsertSpec spec;
        spec.table      = TableType::tableName;
        spec.or_replace = true;
        std::apply([&](auto... col_tags)
        {
            ([&]<typename ColType>(ColType col_tag)
            {
                spec.assignments.emplace_back(
                    ast::AssignmentNode{ .column = std::string(ColType::columnName) }
                );
            }(col_tags), ...);
        }, typename TableType::ColumnsTuple{});
        return spec;
    }

    // Bindings: values only, no SQL
    [[nodiscard]] static ast::BoundParams build_insert_params(const RowType& obj)
    {
        ast::BoundParams params;
        std::apply([&](auto... col_tags)
        {
            ([&]<typename ColType>(ColType col_tag)
            {
                using FT = ColType::FieldType;
                params.push([val = obj.*(ColType::member)](engine::SqliteStatement& s, i32 idx)
                {
                    SqliteTypeAdapter<FT>::bind(s, idx, val);
                });
            }(col_tags), ...);
        }, typename TableType::ColumnsTuple{});
        return params;
    }

    // ----------------------------------------------------------------
    // UPDATE <table> SET col=?, ... WHERE <pk_col>=?, ...
    //
    // Non-PK columns → assignments (SET clause).
    // PK columns     → where predicates (WHERE clause).
    // ----------------------------------------------------------------
    [[nodiscard]] static ast::UpdateSpec build_update_spec(const RowType& obj)
    {
        ast::UpdateSpec spec;
        spec.table = TableType::tableName;

        std::apply([&](auto... col_tags)
        {
            ([&]<typename ColType>(ColType col_tag)
            {
                using FT =  ColType::FieldType;
                if constexpr (ColType::isPrimaryKey)
                {
                    spec.where.emplace_back(ast::PredicateNode{
                        .column = std::string(ColType::columnName),
                        .op     = Compare::Equal,
                    });
                }
                else
                {
                    spec.assignments.emplace_back(ast::AssignmentNode{
                        .column = std::string(ColType::columnName),
                    });
                }
            }(col_tags), ...);
        }, typename TableType::ColumnsTuple{});

        return spec;
    }

    [[nodiscard]] static ast::BoundParams build_update_params(const RowType& obj)
    {
        ast::BoundParams params;
        std::apply([&](auto... col_tags)
        {
            ([&]<typename ColType>(ColType col_tag)
            {
                using FT = ColType::FieldType;
                if constexpr (!ColType::isPrimaryKey)
                {
                    params.push([val = obj.*(ColType::member)](engine::SqliteStatement& s, i32 idx)
                    {
                        SqliteTypeAdapter<FT>::bind(s, idx, val);
                    });
                }
            }(col_tags), ...);

            ([&]<typename ColType>(ColType col_tag)
            {
                using FT = ColType::FieldType;
                if constexpr (ColType::isPrimaryKey)
                {
                    params.push([val = obj.*(ColType::member)](engine::SqliteStatement& s, i32 idx)
                    {
                        SqliteTypeAdapter<FT>::bind(s, idx, val);
                    });
                }
            }(col_tags), ...);
        }, typename TableType::ColumnsTuple{});
        return params;
    }

    // ----------------------------------------------------------------
    // DELETE FROM <table> WHERE <pk_col>=?, ...  (from KeyType value)
    // ----------------------------------------------------------------
    [[nodiscard]] static ast::DeleteSpec build_delete_spec_from_key()
    {
        ast::DeleteSpec spec;
        spec.table = TableType::tableName;

        std::apply([&](auto... col_tags)
        {
            ([&]<typename ColType>(ColType col_tag)
            {
                spec.where.emplace_back(ast::PredicateNode{
                    .column = std::string(ColType::columnName),
                    .op     = Compare::Equal,
                });
            }(col_tags), ...);
        }, typename TableType::KeyTuple{});

        return spec;
    }

    // ----------------------------------------------------------------
    // DELETE FROM <table> WHERE <pk_col>=?, ...  (from RowType object)
    // ----------------------------------------------------------------
    [[nodiscard]] static ast::DeleteSpec build_delete_spec_from_row()
    {
        ast::DeleteSpec spec;
        spec.table = TableType::tableName;

        std::apply([&](auto... col_tags)
        {
            ([&]<typename ColType>(ColType col_tag)
            {
                if constexpr (ColType::isPrimaryKey)
                {
                    spec.where.emplace_back(ast::PredicateNode{
                        .column = std::string(ColType::columnName),
                        .op     = Compare::Equal,
                    });
                }
            }(col_tags), ...);
        }, typename TableType::ColumnsTuple{});

        return spec;
    }

    [[nodiscard]] static ast::BoundParams build_delete_params(const RowType& obj)
    {
        ast::BoundParams params;

        std::apply([&](auto... col_tags)
        {
            ([&]<typename ColType>(ColType col_tag)
            {
                using FT = ColType::FieldType;
                if constexpr (ColType::isPrimaryKey)
                {
                    params.push([val = obj.*(ColType::member)](engine::SqliteStatement& s, i32 idx)
                    {
                        SqliteTypeAdapter<FT>::bind(s, idx, val);
                    });
                }
            }(col_tags), ...);
        }, typename TableType::ColumnsTuple{});
        return params;
    }

    [[nodiscard]] static ast::BoundParams build_delete_params(const KeyType& obj)
    {
        ast::BoundParams params;

        if constexpr (detail::isTuple<KeyType>)
        {
            std::apply([&](auto... values)
            {
                ([&]<typename FT>(FT kv)
                {
                    params.push([val = kv](engine::SqliteStatement& s, i32 idx)
                    {
                        SqliteTypeAdapter<FT>::bind(s, idx, val);
                    });
                }(values), ...);
            }, obj);
        }
        else
        {
            params.push([val = obj](engine::SqliteStatement& s, i32 idx)
            {
                SqliteTypeAdapter<KeyType>::bind(s, idx, val);
            });
        }
        return params;
    }
};
} // namespace tewi
