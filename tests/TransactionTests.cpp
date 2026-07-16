#include <catch2/catch_test_macros.hpp>

#include "Tables.h"

// ============================================================================
//  OrmDatabase::transaction(fn) and the savepoint nesting underneath it.
//
//  Nesting is not a corner case here: the ORM opens transactions of its own
//  (Repository::insert(span) does), so any caller-held transaction wraps one
//  whether the caller knows it or not. Before the guard learned to open a
//  savepoint, that combination threw "cannot start a transaction within a
//  transaction" - i.e. wrapping a batch insert, the very reason to reach for a
//  transaction, was the one thing that could not be done.
// ============================================================================

namespace tewi::tests
{
namespace
{
OrmDatabase seeded()
{
    OrmDatabase db = InMemory();
    db.createTable<UserTable>();
    return db;
}

std::vector<User> rows(i32 from, i32 count)
{
    std::vector<User> out;
    for (i32 i = 0; i < count; ++i)
        out.push_back({.id = from + i, .username = "u" + std::to_string(from + i), .age = 30});
    return out;
}

struct Abort : std::runtime_error
{
    Abort() : std::runtime_error("abort") {}
};
} // namespace

TEST_CASE("transaction: wraps a batch insert that opens its own transaction",
          "[orm][transaction][regression]")
{
    OrmDatabase db = seeded();
    auto users     = db.repo<UserTable>();

    // The regression: Repository::insert(span) opens a transaction internally,
    // so this nests one inside the caller's.
    const auto batch = rows(1, 3);
    db.transaction([&] { users.insert(std::span<const User>(batch)); });

    REQUIRE(db.count<User>() == 3);
}

TEST_CASE("transaction: commits on normal return", "[orm][transaction]")
{
    OrmDatabase db = seeded();
    auto users     = db.repo<UserTable>();

    db.transaction([&] { static_cast<void>(users.insert({.id = 1, .username = "a", .age = 30})); });

    REQUIRE(db.count<User>() == 1);
}

TEST_CASE("transaction: rolls back when the callback throws", "[orm][transaction]")
{
    OrmDatabase db = seeded();
    auto users     = db.repo<UserTable>();

    REQUIRE_THROWS_AS(db.transaction([&] {
                          static_cast<void>(users.insert({.id = 1, .username = "a", .age = 30}));
                          static_cast<void>(users.insert({.id = 2, .username = "b", .age = 31}));
                          throw Abort{};
                      }),
                      Abort);

    // Both inserts are discarded, and the exception still reaches the caller.
    REQUIRE(db.count<User>() == 0);
}

TEST_CASE("transaction: forwards the callback's return value", "[orm][transaction]")
{
    OrmDatabase db = seeded();
    auto users     = db.repo<UserTable>();

    const i64 id = db.transaction([&] { return users.insert({.id = 7, .username = "g", .age = 30}); });
    REQUIRE(id == 7);

    const auto name = db.transaction([&] { return std::string{"value"}; });
    REQUIRE(name == "value");

    // void callbacks compile too - that is the other half of the if constexpr.
    db.transaction([] {});
}

TEST_CASE("transaction: nests", "[orm][transaction]")
{
    OrmDatabase db = seeded();
    auto users     = db.repo<UserTable>();

    SECTION("an inner commit is subsumed by the outer one")
    {
        db.transaction([&] {
            static_cast<void>(users.insert({.id = 1, .username = "a", .age = 30}));
            db.transaction([&] { static_cast<void>(users.insert({.id = 2, .username = "b", .age = 31})); });
        });

        REQUIRE(db.count<User>() == 2);
    }

    SECTION("an outer rollback discards work an inner call committed")
    {
        // The inner RELEASE only merges into the enclosing level; nothing is
        // durable until the outermost COMMIT, so the outer throw discards both.
        REQUIRE_THROWS_AS(db.transaction([&] {
                              db.transaction([&] {
                                  static_cast<void>(users.insert({.id = 1, .username = "a", .age = 30}));
                              });
                              throw Abort{};
                          }),
                          Abort);

        REQUIRE(db.count<User>() == 0);
    }

    SECTION("an inner rollback discards only its own level")
    {
        db.transaction([&] {
            static_cast<void>(users.insert({.id = 1, .username = "keep", .age = 30}));

            // The inner call throws and is caught here, so only its savepoint is
            // rewound; the enclosing transaction stays open and still commits.
            try
            {
                db.transaction([&] {
                    static_cast<void>(users.insert({.id = 2, .username = "drop", .age = 31}));
                    throw Abort{};
                });
            }
            catch (const Abort&)
            {}

            static_cast<void>(users.insert({.id = 3, .username = "also-keep", .age = 32}));
        });

        const auto all = db.select<UserTable>().orderBy<&User::id>().toVector();
        REQUIRE(all.size() == 2);
        REQUIRE(all[0].username == "keep");
        REQUIRE(all[1].username == "also-keep");
    }

    SECTION("nesting three deep")
    {
        db.transaction([&] {
            static_cast<void>(users.insert({.id = 1, .username = "a", .age = 30}));
            db.transaction([&] {
                static_cast<void>(users.insert({.id = 2, .username = "b", .age = 31}));
                db.transaction([&] { static_cast<void>(users.insert({.id = 3, .username = "c", .age = 32})); });
            });
        });

        REQUIRE(db.count<User>() == 3);
    }
}

TEST_CASE("transaction: the connection returns to autocommit afterwards", "[orm][transaction]")
{
    OrmDatabase db = seeded();
    auto users     = db.repo<UserTable>();

    // A savepoint left unreleased would keep the transaction open and silently
    // hold a write lock, so check the guard actually unwinds to a clean state.
    db.transaction([&] { static_cast<void>(users.insert({.id = 1, .username = "a", .age = 30})); });

    try
    {
        db.transaction([&] { throw Abort{}; });
    }
    catch (const Abort&)
    {}

    // If either path leaked a level, this next transaction would misbehave.
    db.transaction([&] { static_cast<void>(users.insert({.id = 2, .username = "b", .age = 31})); });
    REQUIRE(db.count<User>() == 2);
}
} // namespace tewi::tests
