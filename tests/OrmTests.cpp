#include <catch2/catch_test_macros.hpp>

#include "tewi/RegisterTable.h"

import tewi;
import std;

#include "Tables.h"

// ---------------------------------------------------------
// 3. Tests
// ---------------------------------------------------------
namespace tewi::tests
{
TEST_CASE("ORM Concepts: Type Classification Validation", "[orm][concepts]")
{
    SECTION("IsTable Concept")
    {
        // Valid tables should satisfy the concept
        STATIC_REQUIRE(tewi::detail::IsTable<UserTable>);
        STATIC_REQUIRE(tewi::detail::IsTable<PostTable>);

        // Plain structs or arbitrary types should fail the concept
        STATIC_REQUIRE_FALSE(tewi::detail::IsTable<User>);
        STATIC_REQUIRE_FALSE(tewi::detail::IsTable<int>);
    }

    SECTION("HomogeneousMemberPtrs Concept")
    {
        // Valid: All pointers belong to the same struct
        STATIC_REQUIRE(tewi::detail::HomogeneousMemberPtrs<&User::id, &User::username>);

        // Invalid: Mixed pointers from different structs
        STATIC_REQUIRE_FALSE(tewi::detail::HomogeneousMemberPtrs<&User::id, &Post::content>);

        // Invalid: Empty pack (requires at least 1)
        STATIC_REQUIRE_FALSE(tewi::detail::HomogeneousMemberPtrs<>);
    }
}

TEST_CASE("ORM Constraints: Foreign Key Detection", "[orm][constraints]")
{
    SECTION("Directional Foreign Key Resolution")
    {
        // PostTable has a FK pointing to UserTable
        STATIC_REQUIRE(tewi::detail::HasFkTo<PostTable, UserTable>);

        // UserTable does NOT have a FK pointing to PostTable
        STATIC_REQUIRE_FALSE(tewi::detail::HasFkTo<UserTable, PostTable>);
    }

    SECTION("Extracting Foreign Key Column Data")
    {
        // Isolate the column type that holds the FK
        using FkCol = detail::FkColumn<PostTable, UserTable>;

        // Ensure the correct column was found
        STATIC_REQUIRE(FkCol::ColumnName == "author_id");

        // Ensure the reference resolves back to the target table's correct column
        constexpr std::string_view ref_col =
            tewi::detail::fk_referenced_col_name<FkCol, UserTable>();
        STATIC_REQUIRE(ref_col == "id");
    }
}

TEST_CASE("ORM DDL: column list", "[orm][schema]")
{
    SECTION("OrderItemTable column list without prefix")
    {
        std::string cols = OrderItemTable::column_list();
        REQUIRE(cols == "order_id, item_number, description, quantity");
    }

    SECTION("OrderItemTable column list with prefix")
    {
        std::string cols = OrderItemTable::column_list("ug");
        REQUIRE(cols == "ug.order_id, ug.item_number, ug.description, ug.quantity");
    }
}

TEST_CASE("ORM: Schema Creation and Basic CRUD", "[orm][crud]")
{
    // Setup in-memory SQLite DB
    engine::SqliteConnection raw_db(":memory:");
    OrmDatabase db(raw_db);

    // Bootstrap Tables
    tewi::createTablesIfNotExist<UserTable, PostTable>(raw_db);

    auto users = db.repo<UserTable>();

    SECTION("Insert and Read Row")
    {
        User alice{1, "Alice", 28};
        i64 new_id = users.insert(alice);
        REQUIRE(new_id > 0);

        auto fetched = db.select<User>().where<&User::id>(new_id).firstOrDefault();
        REQUIRE(fetched.has_value());
        REQUIRE(fetched->username == "Alice");
        REQUIRE(fetched->age == 28);
    }

    SECTION("Update Row")
    {
        i64 id = users.insert({1, "Bob", 30});

        User bob = db.select<User>().where<&User::id>(id).firstOrDefault().value();
        bob.age  = 31; // Happy Birthday Bob
        users.update(bob);

        auto updated_bob = db.select<User>().where<&User::id>(id).firstOrDefault().value();
        REQUIRE(updated_bob.age == 31);
    }

    SECTION("Delete Row")
    {
        i64 id = users.insert({1, "Charlie", std::nullopt});
        i64 count_before = db.count<User>();
        REQUIRE(count_before == 1);

        users.remove(id);
        i64 count_after = db.count<User>();
        REQUIRE(count_after == 0);
    }
}

TEST_CASE("ORM: Advanced Queries and Filtering", "[orm][select]")
{
    auto raw = engine::InMemory();
    OrmDatabase db(raw);
    tewi::createTablesIfNotExist<UserTable>(raw);

    auto users = db.repo<UserTable>();
    users.insert({
        {1, "Dave", 20},
        {2, "Eve", 25},
        {3, "Frank", 25},
        {4, "Grace", 40}
    });

    SECTION("WhereOp and OrderBy")
    {
        auto results = db.select<User>()
                           .whereOp<&User::age>(">=", 25)
                           .orderBy<&User::username>(Order::DESC)
                           .toVector();

        REQUIRE(results.size() == 3);
        REQUIRE(results[0].username == "Grace"); // 40
        REQUIRE(results[1].username == "Frank"); // 25
        REQUIRE(results[2].username == "Eve");   // 25
    }
}

TEST_CASE("ORM: Foreign Key Joins", "[orm][join]")
{
    auto raw = engine::InMemory();
    OrmDatabase db(raw);
    tewi::createTablesIfNotExist<UserTable, PostTable>(raw);

    i64 id = db.repo<UserTable>().insert({0, "Writer1", 35});

    db.repo<PostTable>().insert({.id=1, .author_id=static_cast<int>(id), .content="Hello World"});
    db.repo<PostTable>().insert({.id=2, .author_id=static_cast<int>(id), .content="Second Post"});

    SECTION("Implicit Auto-FK Join")
    {
        // Because PostTable has a ForeignKey to UserTable, Join() auto-resolves.
        auto joined = db.select<PostTable>().join<UserTable>().toVector();

        REQUIRE(joined.size() == 2);

        // joined is std::vector<std::pair<Post, User>>
        REQUIRE(joined[0].first.content == "Hello World");
        REQUIRE(joined[0].second.username == "Writer1");

        REQUIRE(joined[1].first.content == "Second Post");
        REQUIRE(joined[1].second.username == "Writer1");
    }
}
} // namespace tewi::tests
