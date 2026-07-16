#include <catch2/catch_test_macros.hpp>

#include "Tables.h"

// ============================================================================
//  Regression tests for BasicQuery's state transitions and terminals.
//
//  Every case here failed (or failed to compile) before the correctness pass:
//  the transitions dropped their bound parameter values, the terminals rewrote
//  the spec in place, and begin()/end() never matched the Iterator ctor.
// ============================================================================

namespace tewi::tests
{
namespace
{
/// users: alice(30) bob(25) carol(35), each with one post.
OrmDatabase seeded()
{
    OrmDatabase db = InMemory();
    db.createTable<UserTable, PostTable>();

    auto users = db.repo<UserTable>();
    const i64 alice = users.insert({.id = 1, .username = "alice", .age = 30});
    const i64 bob   = users.insert({.id = 2, .username = "bob", .age = 25});
    const i64 carol = users.insert({.id = 3, .username = "carol", .age = 35});

    auto posts = db.repo<PostTable>();
    static_cast<void>(posts.insert({.id = 1, .author_id = static_cast<i32>(alice), .content = "a"}));
    static_cast<void>(posts.insert({.id = 2, .author_id = static_cast<i32>(bob), .content = "b"}));
    static_cast<void>(posts.insert({.id = 3, .author_id = static_cast<i32>(carol), .content = "c"}));

    return db;
}
} // namespace

TEST_CASE("BasicQuery: params survive a join", "[orm][query][regression]")
{
    OrmDatabase db = seeded();

    SECTION("where() before join() keeps its bound value")
    {
        // The predicate lives in the spec, the value lives in the params. Drop
        // the params and ":p1" binds to nothing, which SQLite reads as NULL:
        // the filter matches no rows and the query silently returns empty.
        const auto rows = db.select<PostTable>()
                              .where<&Post::author_id>(2)
                              .join<UserTable>()
                              .toVector();

        REQUIRE(rows.size() == 1);
        REQUIRE(rows[0].first.content == "b");
        REQUIRE(rows[0].second.username == "bob");
    }

    SECTION("where() on both sides of a join gets distinct param names")
    {
        // p_counter must travel with the params, or the second where() reissues
        // ":p1" and BoundParams::add throws BinderError on the duplicate.
        const auto rows = db.select<PostTable>()
                              .where<&Post::author_id>(3)
                              .join<UserTable>()
                              .where<&User::username>(std::string("carol"))
                              .toVector();

        REQUIRE(rows.size() == 1);
        REQUIRE(rows[0].second.username == "carol");
    }

    SECTION("a filtered join that matches nothing is still empty")
    {
        const auto rows = db.select<PostTable>()
                              .where<&Post::author_id>(999)
                              .join<UserTable>()
                              .toVector();

        REQUIRE(rows.empty());
    }
}

TEST_CASE("BasicQuery: terminals leave the query reusable", "[orm][query][regression]")
{
    OrmDatabase db = seeded();

    SECTION("count() does not turn the query into a COUNT query")
    {
        auto q = db.select<UserTable>().where<&User::age>(Compare::Greater, 26);

        REQUIRE(q.count() == 2);

        // Before the fix the spec was rewritten to CountStar in place, so this
        // hydrated 3 columns off a 1-column result set.
        const auto rows = q.toVector();
        REQUIRE(rows.size() == 2);
        REQUIRE(q.count() == 2);
    }

    SECTION("firstOrDefault() does not make LIMIT 1 sticky")
    {
        auto q = db.select<UserTable>().orderBy<&User::id>();

        const auto first = q.firstOrDefault();
        REQUIRE(first.has_value());
        REQUIRE(first->username == "alice");

        REQUIRE(q.toVector().size() == 3);
    }

    SECTION("an explicit limit survives firstOrDefault()")
    {
        auto q = db.select<UserTable>().orderBy<&User::id>().limit(2);

        REQUIRE(q.firstOrDefault().has_value());
        REQUIRE(q.toVector().size() == 2);
    }

    SECTION("firstOrDefault(fallback) returns the fallback when empty")
    {
        const auto q = db.select<UserTable>().where<&User::username>(std::string("nobody"));

        const User fallback{.id = -1, .username = "none", .age = std::nullopt};
        REQUIRE(q.firstOrDefault(fallback).username == "none");
    }
}

TEST_CASE("BasicQuery: count() honours DISTINCT", "[orm][query][regression]")
{
    OrmDatabase db = InMemory();
    db.createTable<UserTable>();

    auto users = db.repo<UserTable>();
    static_cast<void>(users.insert({.id = 1, .username = "a", .age = 30}));
    static_cast<void>(users.insert({.id = 2, .username = "b", .age = 30}));
    static_cast<void>(users.insert({.id = 3, .username = "c", .age = 25}));

    // Three rows, two distinct ages. The CountStar renderer used to read only
    // the projection kind, so the distinct count came back as the row count.
    REQUIRE(db.select<UserTable>().count() == 3);
    REQUIRE(db.select<UserTable>().distinct<&User::age>().count() == 2);
}

TEST_CASE("BasicQuery: lazy iteration", "[orm][query][regression]")
{
    OrmDatabase db = seeded();

    SECTION("a query models input_range")
    {
        // Iterator advertised input_iterator_tag but had no post-increment, so
        // it failed std::weakly_incrementable. Nothing caught it because
        // begin() itself did not compile.
        STATIC_REQUIRE(std::ranges::input_range<decltype(db.select<UserTable>())>);
    }

    SECTION("a query is usable as a range")
    {
        // begin() passed 3 args to a 4-param Iterator ctor, so this did not
        // compile until the ctor took its params by const&.
        i32 seen = 0;
        for (const auto& user : db.select<UserTable>())
        {
            REQUIRE(!user.username.empty());
            ++seen;
        }
        REQUIRE(seen == 3);
    }

    SECTION("iteration applies the bound params")
    {
        const auto q = db.select<UserTable>().where<&User::age>(Compare::GreaterEqual, 30);

        std::vector<std::string> names;
        for (const auto& user : q) names.push_back(user.username);

        std::ranges::sort(names);
        REQUIRE(names == std::vector<std::string>{"alice", "carol"});
    }
}

TEST_CASE("BasicQuery: OFFSET without LIMIT", "[orm][query][regression]")
{
    OrmDatabase db = seeded();

    // SQLite only accepts OFFSET as a suffix of LIMIT: rendering a bare
    // "OFFSET 1" is a syntax error, so this threw before the renderer fix.
    const auto rows = db.select<UserTable>().orderBy<&User::id>().offset(1).toVector();

    REQUIRE(rows.size() == 2);
    REQUIRE(rows[0].username == "bob");
    REQUIRE(rows[1].username == "carol");
}

TEST_CASE("OrmDatabase: ownership semantics", "[orm][query][regression]")
{
    // Moving is allowed: a factory that builds a database and returns it by
    // value borrows nothing yet, so the move is safe. seeded() relies on this.
    STATIC_REQUIRE(std::is_move_constructible_v<OrmDatabase>);

    // Copying never was possible - SqliteConnection owns a unique handle.
    STATIC_REQUIRE(!std::is_copy_constructible_v<OrmDatabase>);

    // The dangling case - db.select<>() on a temporary - is ruled out by the
    // deleted && overloads. That is a compile error by construction and cannot
    // be asserted here: selecting a deleted function is a hard error rather
    // than an unsatisfied requirement, so `requires` cannot detect it.

    // The lvalue path stays usable.
    STATIC_REQUIRE(requires(OrmDatabase& db) { db.template select<UserTable>(); });
}

TEST_CASE("Table: foreign key counting", "[orm][schema][regression]")
{
    // join<UserTable>() on MessageTable is ambiguous: FindFkTo would silently
    // pick sender_id. fkCountTo is what lets join() diagnose it instead.
    STATIC_REQUIRE(MessageTable::fkCountTo<UserTable> == 2);
    STATIC_REQUIRE(MessageTable::hasFkTo<UserTable>);

    // The unambiguous case still resolves to exactly one.
    STATIC_REQUIRE(PostTable::fkCountTo<UserTable> == 1);

    // No relation in the other direction.
    STATIC_REQUIRE(UserTable::fkCountTo<PostTable> == 0);
    STATIC_REQUIRE(UserTable::fkCountTo<MessageTable> == 0);
}
} // namespace tewi::tests
