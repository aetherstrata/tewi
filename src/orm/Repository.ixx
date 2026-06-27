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
    [[nodiscard]] std::optional<RowType> find(V&& value) const
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
        auto stmt = _db.prepare(cq.sql());
        // No WHERE bindings needed for a plain count
        if (!stmt.step()) return 0;
        return SqliteTypeAdapter<i64>::read(stmt, 0);
    }

    // ----------------------------------------------------------------
    // insert() – INSERT OR REPLACE (single row)
    // ----------------------------------------------------------------
    [[nodiscard]] i64 insert(const RowType& obj) const
    {
        auto [shape, params] = build_insert(obj);
        auto stmt = _db.prepare(ast::compile(shape).sql());
        params.bind(stmt);
        stmt.exec();
        return _db.lastInsertRowId();
    }

    // ----------------------------------------------------------------
    // insert() – INSERT OR REPLACE (batch)
    // ----------------------------------------------------------------
    void insert(const std::span<const RowType>& rows) const
    {
        if (rows.empty()) return;

        auto txn = _db.beginTransaction();

        for (const auto& row : rows)
        {
            insert(row);
        }
        txn.commit();
    }

    // ----------------------------------------------------------------
    // update() – UPDATE by primary key
    // ----------------------------------------------------------------
    void update(const RowType& obj) const
    {
        auto [shape, params] = build_update(obj);
        auto stmt = _db.prepare(ast::compile(shape).sql());
        params.bind(stmt);
        stmt.exec();
    }

    // ----------------------------------------------------------------
    // erase() – DELETE by primary key (variadic composite key)
    // ----------------------------------------------------------------
    template <typename... PKValues>
    requires(sizeof...(PKValues) > 1)
    void erase(PKValues&&... pk_values) const
    {
        erase(KeyType{std::forward<PKValues>(pk_values)...});
    }

    // ----------------------------------------------------------------
    // erase() – DELETE by primary key value (scalar or tuple)
    // ----------------------------------------------------------------
    void erase(KeyType key) const
    {
        auto [spec, params] = build_delete(key);
        auto cq   = ast::compile(spec);
        auto stmt = _db.prepare(cq.sql());
        params.bind(stmt);
        stmt.step();
    }

    // ----------------------------------------------------------------
    // erase() – DELETE by row object (PK fields extracted)
    // ----------------------------------------------------------------
    void erase(const RowType& obj) const
    {
        auto [spec, params] = build_delete(obj);
        auto cq   = ast::compile(spec);
        auto stmt = _db.prepare(cq.sql());
        params.bind(stmt);
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
    [[nodiscard]] static std::pair<ast::InsertSpec, ast::BoundParams>
    build_insert(const RowType& obj)
    {
        ast::InsertSpec  spec;
        ast::BoundParams params;
        spec.table      = TableType::tableName;
        spec.or_replace = true;

        i32 counter = 1;
        std::apply([&](auto... col_tags)
        {
            ([&]<typename ColType>(ColType col_tag)
            {
                using FT = ColType::FieldType;
                std::string name = std::to_string(counter);
                spec.assignments.emplace_back(std::string(ColType::columnName), name);
                params.push(name,
                    [val = obj.*(ColType::member), counter](engine::SqliteStatement& s)
                    {
                        SqliteTypeAdapter<FT>::bind(s, counter, val);
                    });
                counter++;
            }(col_tags), ...);
        }, typename TableType::ColumnsTuple{});

        return {std::move(spec), std::move(params)};
    }

    [[nodiscard]] static std::pair<ast::UpdateSpec, ast::BoundParams>
    build_update(const RowType& obj)
    {
        ast::UpdateSpec  spec;
        ast::BoundParams params;
        spec.table = TableType::tableName;

        i32 counter = 1;
        auto add = [&]<typename ColType>(ColType col_tag, bool is_pk)
        {
            using FT      = ColType::FieldType;
            auto  slot    = counter; // captured integer
            auto  name    = std::to_string(counter++);

            if (!is_pk)
            {
                spec.assignments.emplace_back(
                    ast::AssignmentNode{.column = std::string(ColType::columnName),
                                        .param_name = name});
            } else
            {
                spec.where.emplace_back(
                    ast::PredicateNode{.column = std::string(ColType::columnName),
                                       .op     = Compare::Equal,
                                       .param_name = name});
            }
            params.push(name,
                [val = obj.*(ColType::member), slot]
                (engine::SqliteStatement& s) {
                    SqliteTypeAdapter<FT>::bind(s, slot, val);
                });
        };

        // SET columns first (non-PK), then WHERE columns (PK) -
        // order is now explicit and documented, not implicit.
        std::apply([&](auto... col_tags)
        {
            ([&]<typename ColType>(ColType col_tag)
            {
                if constexpr (!ColType::isPrimaryKey) add(col_tag, false);
            }(col_tags), ...);
            ([&]<typename ColType>(ColType col_tag)
            {
                if constexpr (ColType::isPrimaryKey)  add(col_tag, true);
            }(col_tags), ...);
        }, typename TableType::ColumnsTuple{});

        return {std::move(spec), std::move(params)};
    }

    // ----------------------------------------------------------------
    // DELETE FROM <table> WHERE <pk_col>=?, ...  (from KeyType value)
    // ----------------------------------------------------------------
    [[nodiscard]] static std::pair<ast::DeleteSpec, ast::BoundParams>
    build_delete(const RowType& obj)
    {
        ast::DeleteSpec  spec;
        ast::BoundParams params;
        spec.table = TableType::tableName;

        i32 counter = 1;
        std::apply([&](auto... col_tags)
        {
            ([&]<typename ColType>(ColType col_tag)
            {
                if constexpr (ColType::isPrimaryKey)
                {
                    using FT     = ColType::FieldType;
                    auto  slot   = counter;
                    auto  name   = std::to_string(counter++);
                    spec.where.emplace_back(
                        ast::PredicateNode{.column     = std::string(ColType::columnName),
                                           .op         = Compare::Equal,
                                           .param_name = name});
                    params.push(name,
                        [val = obj.*(ColType::member), slot]
                        (engine::SqliteStatement& s) {
                            SqliteTypeAdapter<FT>::bind(s, slot, val);
                        });
                }
            }(col_tags), ...);
        }, typename TableType::ColumnsTuple{});

        return {std::move(spec), std::move(params)};
    }
    
    [[nodiscard]] static std::pair<ast::DeleteSpec, ast::BoundParams>
    build_delete(const KeyType& key)
    {
        ast::DeleteSpec  spec;
        ast::BoundParams params;
        spec.table = TableType::tableName;

        i32 counter = 1;

        if constexpr (detail::isTuple<KeyType>)
        {
            [&]<std::size_t... Is>(std::index_sequence<Is...>)
            {
                ([&] {
                    using ColType = std::tuple_element_t<Is, typename TableType::KeyTuple>;
                    using FT      = decltype(std::get<Is>(key));
                    auto  slot    = counter;
                    auto  name    = std::to_string(counter++);
                    spec.where.emplace_back(
                        ast::PredicateNode{
                            .column     = std::string(ColType::columnName),
                            .op         = Compare::Equal,
                            .param_name = name});
                    params.push(name,
                        [val = std::get<Is>(key), slot](engine::SqliteStatement& s) {
                            SqliteTypeAdapter<FT>::bind(s, slot, val);
                        });
                }(), ...);
            }(std::make_index_sequence<std::tuple_size_v<typename TableType::KeyTuple>>{});
        }
        else
        {
            // Scalar PK - unchanged
            auto name = std::to_string(counter);
            auto slot = counter++;
            std::apply([&]<typename ColType>(ColType col_tag)
            {
                spec.where.emplace_back(
                    ast::PredicateNode{
                        .column     = std::string(ColType::columnName),
                        .op         = Compare::Equal,
                        .param_name = name});
            }, typename TableType::KeyTuple{});
            params.push(name,
                [val = key, slot](engine::SqliteStatement& s) {
                    SqliteTypeAdapter<KeyType>::bind(s, slot, val);
                });
        }

        return {std::move(spec), std::move(params)};
    }
};
} // namespace tewi
