export module tewi:repository;

import :constraint_helpers;
import :select;
import :sqlite_connection;
import :sqlite_statement;
import :sqlite_transaction;

import std;

// ============================================================================
//  §16  Repository<Table>  -  CRUD + Count + BulkInsert
// ============================================================================
namespace tewi
{
/// Full CRUD operations for a single table type.
export template <typename TableType>
requires detail::IsTable<TableType>
class Repository
{
public:
    using RowType = TableType::RowType;

    explicit Repository(engine::SqliteConnection& db)
        : _db(db)
    {}

    // ----------------------------------------------------------------
    //  Schema management
    // ----------------------------------------------------------------

    void createTable(bool if_not_exists = true)
    {
        _db.exec(TableType::create_table_sql(if_not_exists));
        // Indexes are separate statements - SQLite requires this.
        if constexpr (TableType::IndexCount > 0)
        {
            _db.exec(TableType::create_indexes_sql(if_not_exists));
        }
    }

    void dropTable(bool if_exists = true)
    {
        _db.exec("DROP TABLE " + std::string(if_exists ? "IF EXISTS " : "")
                 + std::string(TableType::TableName) + ";");
    }

    // ----------------------------------------------------------------
    //  INSERT
    // ----------------------------------------------------------------

    /// Insert a single row.  Returns the rowid assigned by SQLite.
    i64 insert(const RowType& obj)
    {
        auto stmt = _db.prepare(insert_sql());
        TableType::bind_all(stmt, obj, 1);
        stmt.exec();
        return _db.lastInsertRowId();
    }

    /// Bulk-insert many rows inside a single transaction.
    void insert(const std::vector<RowType>& rows)
    {
        if (rows.empty()) return;
        auto txn  = _db.beginTransaction();
        auto stmt = _db.prepare(insert_sql());
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
    //  UPDATE
    // ----------------------------------------------------------------

    /// Update all non-PK columns for the row identified by its PK.
    void update(const RowType& obj)
    requires detail::HasPrimaryKey<TableType>
    {
        auto stmt = _db.prepare(update_sql());
        i32 idx   = 1;

        // Bind non-PK fields first (SET clause)
        std::apply([&](auto... cols)
        {
            ([&]<typename Col>(Col)
            {
                if constexpr (!Col::IsPrimaryKey)
                {
                    using FT  = Col::FieldType;
                    const auto& val = obj.*(Col::MemberPtr);
                    SqliteTypeAdapter<FT>::bind(stmt, idx++, val);
                }
            }(cols), ...);
        }, typename TableType::ColumnsTuple{});

        // Bind PK last (WHERE clause)
        std::apply([&](auto... cols)
        {
            ([&]<typename Col>(Col)
            {
                if constexpr (Col::IsPrimaryKey)
                {
                    using FT  = Col::FieldType;
                    const auto& val = obj.*(Col::MemberPtr);
                    SqliteTypeAdapter<FT>::bind(stmt, idx++, val);
                }
            }(cols), ...);
        }, typename TableType::ColumnsTuple{});

        stmt.exec();
    }

    /// Bulk-update inside a single transaction.
    void update(const std::vector<RowType>& rows)
    {
        if (rows.empty()) return;
        auto txn = _db.beginTransaction();
        for (const auto& row : rows)
        {
            this->update(row);
        }
        txn.commit();
    }

    // ----------------------------------------------------------------
    //  DELETE
    // ----------------------------------------------------------------

    /// Delete the row with the given primary key value.
    template <typename PKType>
    requires detail::HasPrimaryKey<TableType> && SqliteAdaptable<PKType>
    void remove(const PKType& pk_value)
    {
        const std::string sql = "DELETE FROM " + std::string(TableType::TableName) + " WHERE "
                                + std::string(TableType::pk_name) + " = ?;";
        auto stmt = _db.prepare(sql);
        SqliteTypeAdapter<PKType>::bind(stmt, 1, pk_value);
        stmt.exec();
    }

    /// Delete rows matching a prebuilt QueryState (from SelectQuery::state()).
    void removeWhere(const detail::QueryState& qs)
    {
        const std::string sql =
            "DELETE FROM " + std::string(TableType::TableName) + qs.where_sql() + ";";
        auto stmt = _db.prepare(sql);
        qs.bind_where(stmt);
        stmt.exec();
    }

    // ----------------------------------------------------------------
    //  COUNT
    // ----------------------------------------------------------------

    [[nodiscard]] i64 count()
    {
        const std::string sql = "SELECT COUNT(*) FROM " + std::string(TableType::TableName) + ";";
        auto stmt             = _db.prepare(sql);
        stmt.step();
        return stmt.columnLong(0);
    }

    [[nodiscard]] i64 countWhere(const detail::QueryState& qs)
    {
        const std::string sql =
            "SELECT COUNT(*) FROM " + std::string(TableType::TableName) + qs.where_sql() + ";";
        auto stmt = _db.prepare(sql);
        qs.bind_where(stmt);
        stmt.step();
        return stmt.columnLong(0);
    }

    // ----------------------------------------------------------------
    //  SELECT shorthand (delegates to SelectQuery)
    // ----------------------------------------------------------------
    [[nodiscard]] SelectQuery<TableType> select() { return SelectQuery<TableType>(_db); }

private:
    engine::SqliteConnection& _db;

    [[nodiscard]] std::string insert_sql() const
    {
        std::string columns{};
        std::string queryState{};
        bool first = true;
        std::apply([&](auto... c)
        {
            ([&]<typename Col>(Col)
            {
                if (!first)
                {
                    columns += ", ";
                    queryState   += ", ";
                }
                columns  += std::string(Col::ColumnName);
                queryState    += "?";
                first  = false;
            }(c), ...);
        }, typename TableType::ColumnsTuple{});
        return "INSERT INTO " + std::string(TableType::TableName) + " (" + columns + ") VALUES (" + queryState
               + ");";
    }

    [[nodiscard]] std::string update_sql() const
    {
        std::string set_clause{};
        bool first = true;
        std::apply([&](auto... c)
        {
            ([&]<typename Col>(Col)
            {
                if constexpr (!Col::IsPrimaryKey)
                {
                    if (!first) set_clause += ", ";
                    set_clause += std::string(Col::ColumnName) + " = ?";
                    first       = false;
                }
            }(c), ...);
        }, typename TableType::ColumnsTuple{});
        return "UPDATE " + std::string(TableType::TableName) + " SET " + set_clause + " WHERE "
               + std::string(TableType::pk_name) + " = ?;";
    }
};
} //namespace tewi