export module tewi:orm_database;

import :member_traits;
import :join;
import :projection;
import :repository;
import :sqlite_connection;
import :select;

import std;

// ============================================================================
//  §17  OrmDatabase  - ORM-aware façade over SqliteDatabase
// ============================================================================
namespace tewi
{
/// A thin non-owning façade that adds compile-time ORM entry-points on top of
/// an existing SqliteDatabase.  The underlying database must outlive this object.
export class OrmDatabase
{
public:
    explicit OrmDatabase(engine::SqliteConnection& db) noexcept
        : _db(db)
    {}

    // ----------------------------------------------------------------
    //  Select<RowType>()  - requires ORM_REGISTER_TABLE
    // ----------------------------------------------------------------
    /// Returns a SelectQuery for the registered table of RowType.
    template <typename RowType>
        requires detail::HasRegisteredTable<RowType>
    [[nodiscard]] auto select() const
    {
        using TT = detail::TableRegistry<RowType>::TableType;
        return SelectQuery<TT>(_db);
    }

    // ----------------------------------------------------------------
    //  Select<TableType>() - explicit Table type (no registration needed)
    // ----------------------------------------------------------------
    template <typename TableType>
        requires detail::IsTable<TableType>
    [[nodiscard]] SelectQuery<TableType> select() const
    {
        return SelectQuery<TableType>(_db);
    }

    // Existing: Select<User>()  or  Select<UserTable>()
    // New:      Select<&User::id, &User::name>()

    template <auto... MemberPtrs>
        requires detail::HomogeneousMemberPtrs<MemberPtrs...>
    [[nodiscard]] auto select() const
    {
        using ObjType = detail::projection_object_t<MemberPtrs...>;
        static_assert(detail::HasRegisteredTable<ObjType>,
                      "Select<MPs...>: row type not registered.");
        using TT = detail::TableRegistry<ObjType>::TableType;
        return ColumnProjectionQuery<TT, MemberPtrs...>(_db);
    }

    // ----------------------------------------------------------------
    //  Count<RowType>()
    // ----------------------------------------------------------------
    template <typename RowType>
        requires detail::HasRegisteredTable<RowType>
    [[nodiscard]] i64 count() const
    {
        using TT = detail::TableRegistry<RowType>::TableType;
        return Repository<TT>(_db).count();
    }

    // ----------------------------------------------------------------
    //  Repo<TableType>()  - full CRUD repository
    // ----------------------------------------------------------------
    template <typename TableType>
        requires detail::IsTable<TableType>
    [[nodiscard]] Repository<TableType> repo()
    {
        return Repository<TableType>(_db);
    }

    // ----------------------------------------------------------------
    //  JoinOn<&L::field, &R::field>()  - explicit-condition INNER JOIN
    // ----------------------------------------------------------------
    /// Builds a JoinQuery with an ON clause derived from two member pointers.
    template <auto LeftMember, auto RightMember>
    [[nodiscard]] auto joinOn()
    {
        using LObj = detail::member_ptr<decltype(LeftMember)>::ObjectType;
        using RObj = detail::member_ptr<decltype(RightMember)>::ObjectType;

        static_assert(detail::HasRegisteredTable<LObj> && detail::HasRegisteredTable<RObj>,
                      "JoinOn<>: both row types must be registered with ORM_REGISTER_TABLE.");

        using LT = detail::TableRegistry<LObj>::TableType;
        using RT = detail::TableRegistry<RObj>::TableType;

        constexpr std::string_view lc = LT::template column_name_for<LeftMember>();
        constexpr std::string_view rc = RT::template column_name_for<RightMember>();

        static_assert(!lc.empty() && !rc.empty(),
                      "JoinOn<>: one or both member pointers are not mapped to columns.");

        return JoinQuery<LT, RT>(_db, {}, lc, rc);
    }

    // ----------------------------------------------------------------
    //  Transactions
    // ----------------------------------------------------------------
    [[nodiscard]] engine::SqliteTransaction beginTransaction() { return _db.beginTransaction(); }

    // ----------------------------------------------------------------
    //  Raw access
    // ----------------------------------------------------------------
    [[nodiscard]] engine::SqliteConnection& rawAccess() noexcept { return _db; }

private:
    engine::SqliteConnection& _db;
};

// ============================================================================
//  §18  DDL bootstrap helper
// ============================================================================

/// Execute CREATE TABLE IF NOT EXISTS for every supplied Table type.
/// Typically called once at application startup, after migrations.
export template <typename... Tables>
requires(detail::IsTable<Tables> && ...)
void createTablesIfNotExist(engine::SqliteConnection& db)
{
    ([&]<typename T>()
    {
        db.exec(T::create_table_sql(true));
        if constexpr (T::IndexCount > 0)
        {
            db.exec(T::create_indexes_sql(true));
        }
    }.template operator()<Tables>(), ...);
}
} // namespace tewi
