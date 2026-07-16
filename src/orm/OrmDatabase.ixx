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
/**
 * @brief Entry point to the ORM: owns a database connection and hands out
 *        typed query builders and repositories over it.
 *
 * @details Obtain one from @c InMemory() or @c Open(), then reach the typed
 * API through @c select(), @c repo() and @c count(). No SQL text is
 * written here: read-side calls delegate to @c BasicQuery, and @c createTable()
 * renders DDL from each table descriptor's AST.
 *
 * @par Lifetime
 * The objects returned by @c select() and @c repo() *borrow* this database's
 * connection by reference; they do not keep it alive. The database must
 * therefore outlive every query and repository built from it. Those accessors
 * constrain their explicit object parameter to an lvalue, which turns the easy
 * mistake into a compile error rather than a dangling reference:
 * @code{.cpp}
 * auto q = tewi::InMemory().select<User>();  // ill-formed: borrows a temporary
 *
 * tewi::OrmDatabase db = tewi::InMemory();   // correct: db outlives the query
 * auto q = db.select<User>();
 * @endcode
 * Moving a database that already has live queries against it is not diagnosed,
 * and leaves those queries referring to a moved-from connection.
 *
 * @par Thread safety
 * A single @c OrmDatabase must not be used concurrently from multiple threads
 * without external synchronization, and neither must anything it hands out:
 * queries and repositories share its one underlying connection. Create one
 * @c OrmDatabase per thread, or wrap access in a mutex.
 */
export class OrmDatabase
{
public:
    /**
     * @brief Takes ownership of an open connection.
     *
     * @details Prefer the @c InMemory() and @c Open() factories; this
     * constructor is the seam for a connection opened with non-default flags.
     *
     * @param[in] db  An open connection. Moved from.
     */
    explicit OrmDatabase(engine::SqliteConnection db) noexcept
        : _db(std::move(db))
    {}

    /// @brief Deleted copy constructor. The underlying connection is move-only.
    OrmDatabase(const OrmDatabase&) = delete;
    /// @brief Deleted copy-assignment. The underlying connection is move-only.
    OrmDatabase& operator=(const OrmDatabase&) = delete;

    /**
     * @brief Move constructor. Transfers ownership of the connection.
     *
     * @details Moving is permitted so that a factory can build a database and
     * return it by value - nothing borrows from it at that point, so the move is
     * safe, and a named-local return needs an accessible move constructor even
     * when NRVO elides it. Moving a database with live queries against it is the
     * caller's responsibility; see the class's Lifetime section.
     */
    OrmDatabase(OrmDatabase&&) = default;
    /// @brief Move-assignment. Carries the same caveat as the move constructor.
    OrmDatabase& operator=(OrmDatabase&&) = default;

    /**
     * @brief Starts a query over the table registered for @p Row.
     *
     * @tparam Row  A row struct registered with @c TEWI_REGISTER_TABLE.
     * @tparam Self Deduced explicit object parameter, constrained to an lvalue
     *              reference: the returned query borrows this database, so the
     *              call is ill-formed on a temporary. Const lvalues are accepted.
     *
     * @return A @c BasicQuery selecting every column of @p Row's table.
     */
    template <HasRegisteredTable Row, typename Self>
    requires std::is_lvalue_reference_v<Self>
    [[nodiscard]] auto select(this Self&& self)
    {
        return detail::make_basic_query<typename TableRegistry<Row>::TableType>(self._db);
    }

    /**
     * @brief Starts a query over an explicitly named table descriptor.
     *
     * @details Equivalent to the @p Row overload, but names the @c Table<>
     * directly. Useful where the row type is not registered, or where two
     * descriptors map the same row struct.
     *
     * @tparam TableType A @c Table<> descriptor.
     * @tparam Self      Deduced explicit object parameter; lvalue only.
     *
     * @return A @c BasicQuery selecting every column of @p TableType.
     */
    template <ITable TableType, typename Self>
    requires std::is_lvalue_reference_v<Self>
    [[nodiscard]] auto select(this Self&& self)
    {
        return detail::make_basic_query<TableType>(self._db);
    }

    /**
     * @brief Starts a query projecting only the named columns.
     *
     * @details The result type is a tuple of the members' field types rather
     * than the row struct - a single member pointer projects to that field's
     * type alone.
     *
     * @tparam MemberPtrs Member pointers into one row struct, e.g.
     *                    @c &User::id, @c &User::username. All must name the
     *                    same owner type, whose table must be registered.
     * @tparam Self       Deduced explicit object parameter; lvalue only.
     *
     * @return A @c BasicQuery yielding the projected columns.
     *
     * @par Example
     * @code{.cpp}
     * for (auto [id, name] : db.select<&User::id, &User::username>().toVector())
     *     ...
     * @endcode
     */
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

    /**
     * @brief Returns the CRUD repository for a table.
     *
     * @tparam TableType A @c Table<> descriptor.
     * @tparam Self      Deduced explicit object parameter, constrained to an
     *                   lvalue reference: the repository borrows this database,
     *                   so the call is ill-formed on a temporary.
     *
     * @return A @c Repository<TableType> for insert/update/erase/find.
     */
    template <ITable TableType, typename Self>
    requires std::is_lvalue_reference_v<Self>
    [[nodiscard]] auto repo(this Self&& self)
    {
        return Repository<TableType>{self._db};
    }

    /**
     * @brief Counts every row in the table registered for @p Row.
     *
     * @tparam Row  A row struct registered with @c TEWI_REGISTER_TABLE.
     * @tparam Self Deduced explicit object parameter; lvalue only.
     *
     * @return The table's total row count.
     *
     * @note Unfiltered. For a filtered count, build the query and count that:
     *       @c db.select<Row>().where<&Row::x>(v).count().
     *
     * @throws SqliteError if the statement fails to prepare or step.
     */
    template <typename Row, typename Self>
    requires HasRegisteredTable<Row> && std::is_lvalue_reference_v<Self>
    [[nodiscard]] i64 count(this Self&& self)
    {
        return self.template repo<typename TableRegistry<Row>::TableType>().count();
    }

    /**
     * @brief Runs @p fn inside a transaction: commits when it returns, rolls
     *        back if it throws.
     *
     * @details Whatever @p fn writes lands atomically or not at all. There is no
     * commit to remember and no guard to keep alive, and the transaction cannot
     * outlive the call - which is why no SQLite type appears in this signature.
     *
     * To abandon the work deliberately, throw: the exception rolls the
     * transaction back and then propagates to the caller.
     *
     * Calls nest. An inner @c transaction() opens a savepoint rather than a
     * second @c BEGIN, so it composes both with itself and with the transactions
     * the ORM opens internally (batch insert does). An inner call that throws
     * discards only its own work, and the enclosing one may still commit.
     *
     * @tparam Self Deduced explicit object parameter; lvalue only, matching the
     *              rest of the class.
     * @tparam F    Callable taking no arguments.
     *
     * @param[in] fn  Work to run inside the transaction.
     *
     * @return Whatever @p fn returns, forwarded unchanged; @c void if it returns
     *         nothing. The value is produced before the commit, so a commit
     *         failure throws rather than returning it.
     *
     * @throws SqliteError if the transaction cannot be opened or committed.
     * @throws Anything @p fn throws, after rolling back.
     *
     * @par Example
     * @code{.cpp}
     * db.transaction([&] {
     *     users.insert(rows);
     *     posts.insert(morePosts);
     * });                            // both land, or neither
     *
     * const i64 n = db.transaction([&] { return db.count<User>(); });
     * @endcode
     */
    template <typename Self, std::invocable F>
    requires std::is_lvalue_reference_v<Self>
    std::invoke_result_t<F> transaction(this Self&& self, F&& fn)
    {
        auto txn = self._db.beginTransaction();

        if constexpr (std::is_void_v<std::invoke_result_t<F>>)
        {
            std::invoke(std::forward<F>(fn));
            txn.commit();
        }
        else
        {
            // Named, so the commit happens before the value leaves: a throwing
            // commit must not be masked by a successful-looking return. The
            // forward preserves fn's own return category rather than decaying
            // it, which a plain `auto` here would.
            std::invoke_result_t<F> result = std::invoke(std::forward<F>(fn));
            txn.commit();
            return std::forward<std::invoke_result_t<F>>(result);
        }
    }

    /**
     * @brief Creates each table and its declared indices.
     *
     * @details Renders `CREATE TABLE` from every descriptor's AST, then a
     * `CREATE INDEX` per index the descriptor declares, one descriptor at a time
     * in pack order.
     *
     * @tparam Tables The @c Table<> descriptors to create. Order is free: SQLite
     *                resolves a @c REFERENCES clause when rows are written, not
     *                when the table is declared, so a child table may be created
     *                before its parent.
     *
     * @param[in] ifNotExists  When @c true (default), emits
     *                         `IF NOT EXISTS` and existing tables are left
     *                         untouched. When @c false, re-creating an existing
     *                         table throws.
     *
     * @throws SqliteError if any statement fails.
     *
     * @note Unlike the accessors above, this is callable on a temporary. That is
     *       deliberate and safe: it borrows nothing out, so nothing can dangle -
     *       the call is merely pointless on a database about to be destroyed.
     *
     * @note Creation only. Nothing reconciles a descriptor against the schema of
     *       an existing database, so the two can drift undiagnosed.
     */
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
    /// The owned connection. @c mutable so the const accessors above can borrow
    /// it out non-const: constness of the façade says nothing about the
    /// statements a query will run.
    mutable engine::SqliteConnection _db;
};

/**
 * @brief Creates a transient in-memory database.
 *
 * @return A new @c OrmDatabase backed by @c ":memory:".
 *
 * @note Each in-memory database is independent and ceases to exist as soon as
 *       the database is destroyed.
 */
export OrmDatabase InMemory()
{
    return OrmDatabase(engine::SqliteConnection(":memory:"));
}

/**
 * @brief Opens, or creates, a database at the given path.
 *
 * @details Opens with the default flags (read/write, creating the file when
 * absent), enabling WAL journalling and foreign-key enforcement.
 *
 * @param[in] dbPath  Filesystem path to the database file.
 *
 * @return A new @c OrmDatabase connected to @p dbPath.
 *
 * @throws SqliteError if the file cannot be opened, or if either initial
 *         @c PRAGMA fails.
 */
export OrmDatabase Open(const std::filesystem::path& dbPath)
{
    return OrmDatabase(engine::SqliteConnection(dbPath.c_str()));
}
} // namespace tewi
