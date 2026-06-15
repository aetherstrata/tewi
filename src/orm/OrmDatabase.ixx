// ============================================================================
// OrmDatabase.ixx  -  tewi:orm_database
//
// Pure entry-point façade.  No SQL strings live here: every read-side
// operation delegates to BasicQuery.  DDL helpers still emit strings
// because CREATE TABLE / CREATE INDEX are schema-level, not query-level.
// ============================================================================
export module tewi:orm_database;

import :basic_query;
import :row_hydrator;
import :ast_spec;
import :registry;
import :member_traits;
import :repository;
import :sqlite_connection;
import :table;

import std;

namespace tewi
{
export class OrmDatabase
{
public:
    explicit OrmDatabase(engine::SqliteConnection& db) noexcept
        : _db(db)
    {}

    // ----------------------------------------------------------------
    // select<Row>() - registered row type -> SelectQuery equivalent
    // ----------------------------------------------------------------
    template <HasRegisteredTable Row>
    [[nodiscard]] auto select() const
    {
        return detail::make_basic_query<typename TableRegistry<Row>::TableType>(_db);
    }

    // ----------------------------------------------------------------
    // select<TableType>() - explicit table descriptor
    // ----------------------------------------------------------------
    template <ITable TableType>
    [[nodiscard]] auto select() const
    {
        return detail::make_basic_query<TableType>(_db);
    }

    // ----------------------------------------------------------------
    // select<&T::col1, &T::col2, …>()  - projected columns
    // ----------------------------------------------------------------
    template <auto... MemberPtrs>
        requires detail::HomogeneousMemberPtrs<MemberPtrs...>
    [[nodiscard]] auto select() const
    {
        using Obj = detail::ObjectOf<detail::firstOf<MemberPtrs...>>;
        static_assert(HasRegisteredTable<Obj>,
                      "select<MemberPtrs...>: owner row type is not registered.");

        return detail::make_basic_query<typename TableRegistry<Obj>::TableType>(_db)
            .template select<MemberPtrs...>();
    }

    // ----------------------------------------------------------------
    // repo<TableType>() - full CRUD repository
    // ----------------------------------------------------------------
    template <ITable TableType>
    [[nodiscard]] auto repo() const
    {
        return Repository<TableType>{_db};
    }

    // ----------------------------------------------------------------
    // count<Row>() - convenience shorthand
    // ----------------------------------------------------------------
    template <typename Row>
        requires HasRegisteredTable<Row>
    [[nodiscard]] i64 count() const
    {
        return repo<typename TableRegistry<Row>::TableType>().count();
    }

    // ----------------------------------------------------------------
    // Transactions / raw access
    // ----------------------------------------------------------------
    [[nodiscard]] engine::SqliteTransaction beginTransaction() { return _db.beginTransaction(); }

    [[nodiscard]] engine::SqliteConnection& rawAccess() noexcept { return _db; }

    template <ITable... Tables>
    void createTable(bool ifNotExists = true)
    {
        ([&]()
        {
            _db.exec(Tables::create_table_sql(ifNotExists));
            if constexpr (Tables::indexCount > 0) _db.exec(Tables::create_indexes_sql(ifNotExists));
        }(), ...);
    }

private:
    engine::SqliteConnection& _db;
};
} // namespace tewi
