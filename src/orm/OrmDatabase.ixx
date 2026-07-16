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
    explicit OrmDatabase(engine::SqliteConnection db) noexcept
        : _db(std::move(db))
    {}

    // Repository and BasicQuery borrow _db by reference, so everything this
    // class hands out is tied to this object's address.
    //
    // Copying is impossible regardless - SqliteConnection is move-only. Moving
    // stays available because returning a freshly built database from a factory
    // is both common and safe: nothing borrows from it yet. Moving a database
    // that already has live queries against it leaves those queries pointing at
    // a moved-from connection, and is the caller's responsibility.
    OrmDatabase(const OrmDatabase&)            = delete;
    OrmDatabase& operator=(const OrmDatabase&) = delete;
    OrmDatabase(OrmDatabase&&)                 = default;
    OrmDatabase& operator=(OrmDatabase&&)      = default;

    // The && overloads below are the case worth ruling out by construction: a
    // query borrowing from a temporary database dangles as soon as the
    // full-expression ends, and that one is easy to write by accident.

    // ----------------------------------------------------------------
    // select<Row>() - registered row type -> SelectQuery equivalent
    // ----------------------------------------------------------------
    template <HasRegisteredTable Row>
    [[nodiscard]] auto select() const &
    {
        return detail::make_basic_query<typename TableRegistry<Row>::TableType>(_db);
    }

    template <HasRegisteredTable Row>
    auto select() const && = delete;

    // ----------------------------------------------------------------
    // select<TableType>() - explicit table descriptor
    // ----------------------------------------------------------------
    template <ITable TableType>
    [[nodiscard]] auto select() const &
    {
        return detail::make_basic_query<TableType>(_db);
    }

    template <ITable TableType>
    auto select() const && = delete;

    // ----------------------------------------------------------------
    // select<&T::col1, &T::col2, ...>()  - projected columns
    // ----------------------------------------------------------------
    template <auto... MemberPtrs>
        requires detail::HomogeneousMemberPtrs<MemberPtrs...>
    [[nodiscard]] auto select() const &
    {
        using Obj = detail::ObjectOf<detail::firstOf<MemberPtrs...>>;
        static_assert(HasRegisteredTable<Obj>,
                      "select<MemberPtrs...>: owner row type is not registered.");

        return detail::make_basic_query<typename TableRegistry<Obj>::TableType>(_db)
            .template select<MemberPtrs...>();
    }

    template <auto... MemberPtrs>
        requires detail::HomogeneousMemberPtrs<MemberPtrs...>
    auto select() const && = delete;

    // ----------------------------------------------------------------
    // repo<TableType>() - full CRUD repository
    // ----------------------------------------------------------------
    template <ITable TableType>
    [[nodiscard]] auto repo() const &
    {
        return Repository<TableType>{_db};
    }

    template <ITable TableType>
    auto repo() const && = delete;

    // ----------------------------------------------------------------
    // count<Row>() - convenience shorthand
    // ----------------------------------------------------------------
    template <typename Row>
        requires HasRegisteredTable<Row>
    [[nodiscard]] i64 count() const &
    {
        return repo<typename TableRegistry<Row>::TableType>().count();
    }

    template <typename Row>
        requires HasRegisteredTable<Row>
    i64 count() const && = delete;

    // ----------------------------------------------------------------
    // Transactions / raw access
    //
    // Both hand out a borrow of _db, so both delete their && overload for the
    // same reason as select()/repo() above.
    // ----------------------------------------------------------------
    [[nodiscard]] engine::SqliteTransaction beginTransaction() & { return _db.beginTransaction(); }

    engine::SqliteTransaction beginTransaction() && = delete;

    [[nodiscard]] engine::SqliteConnection& rawAccess() & noexcept { return _db; }

    engine::SqliteConnection& rawAccess() && = delete;

    template <ITable... Tables>
    void createTable(bool ifNotExists = true)
    {
        ([&]()
        {
            const ast::TableDefNode spec = Tables::creation_spec(ifNotExists);
            _db.exec(ast::render(spec));

            for (const ast::IndexDefNode& idxSpec : spec.indices)
            {
                _db.exec(ast::render(idxSpec));
            }
        }(), ...);
    }

private:
    mutable engine::SqliteConnection _db;
};

/**
 * Create a temporary in-memory database.
 * @return A new @c OrmDatabase instance connected to an in-memory database.
 * @note Each in-memory database is independent and ceases to exist as soon as
 *       the database is destroyed.
 */
export OrmDatabase InMemory() noexcept
{
    return OrmDatabase(engine::SqliteConnection(":memory:"));
}

export OrmDatabase Open(const std::filesystem::path& dbPath)
{
    return OrmDatabase(engine::SqliteConnection(dbPath.c_str()));
}
} // namespace tewi
