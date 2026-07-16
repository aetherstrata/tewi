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
    template <HasRegisteredTable Row, typename Self>
    requires std::is_lvalue_reference_v<Self>
    [[nodiscard]] auto select(this Self&& self)
    {
        return detail::make_basic_query<typename TableRegistry<Row>::TableType>(self._db);
    }

    // ----------------------------------------------------------------
    // select<TableType>() - explicit table descriptor
    // ----------------------------------------------------------------
    template <ITable TableType, typename Self>
    requires std::is_lvalue_reference_v<Self>
    [[nodiscard]] auto select(this Self&& self)
    {
        return detail::make_basic_query<TableType>(self._db);
    }

    // ----------------------------------------------------------------
    // select<&T::col1, &T::col2, ...>()  - projected columns
    // ----------------------------------------------------------------
    template <auto... MemberPtrs, typename Self>
    requires detail::HomogeneousMemberPtrs<MemberPtrs...>
          && std::is_lvalue_reference_v<Self>
    [[nodiscard]] auto select(this Self&& self)
    {
        using Obj = detail::ObjectOf<detail::firstOf<MemberPtrs...>>;
        static_assert(HasRegisteredTable<Obj>,
                      "select<MemberPtrs...>: owner row type is not registered.");

        return detail::make_basic_query<typename TableRegistry<Obj>::TableType>(self._db)
            .template select<MemberPtrs...>();
    }

    // ----------------------------------------------------------------
    // repo<TableType>() - full CRUD repository
    // ----------------------------------------------------------------
    template <ITable TableType, typename Self>
    requires std::is_lvalue_reference_v<Self>
    [[nodiscard]] auto repo(this Self&& self)
    {
        return Repository<TableType>{self._db};
    }

    // ----------------------------------------------------------------
    // count<Row>() - convenience shorthand
    // ----------------------------------------------------------------
    template <typename Row, typename Self>
    requires HasRegisteredTable<Row> && std::is_lvalue_reference_v<Self>
    [[nodiscard]] i64 count(this Self&& self)
    {
        return self.template repo<typename TableRegistry<Row>::TableType>().count();
    }

    // ----------------------------------------------------------------
    // Transactions / raw access
    //
    // Both hand out a borrow of _db, so both delete their && overload for the
    // same reason as select()/repo() above.
    // ----------------------------------------------------------------
    template <typename Self>
    requires std::is_lvalue_reference_v<Self>
    [[nodiscard]] engine::SqliteTransaction beginTransaction(this Self&& self)
    {
        return self._db.beginTransaction();
    }

    template <typename Self>
    requires std::is_lvalue_reference_v<Self>
    [[nodiscard]] engine::SqliteConnection& rawAccess(this Self&& self) noexcept
    {
        return self._db;
    }

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
