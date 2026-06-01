#include <catch2/catch_test_macros.hpp>

#include "tewi/RegisterTable.h"

import tewi;
import std;

namespace tewi::tests
{
// ---------------------------------------------------------
// 1. Define C++ Structs
// ---------------------------------------------------------
struct User
{
    int id = 0;
    std::string username;
    std::optional<int> age;
};

struct Post
{
    int id        = 0;
    int author_id = 0;
    std::string content;
};

struct Author
{
    int id;
    std::string name;
};

struct Book
{
    int id;
    int author_id;
    std::string title;
};

struct UnregisteredEntity
{
    int dummy;
};

// ---------------------------------------------------------
// 2. Map ORM Tables
// ---------------------------------------------------------
using UserTable = Table<"users", User,
    Columns<
        Column<"id", &User::id, PrimaryKey<>>,
        Column<"username", &User::username, NotNull, Unique>,
        Column<"age", &User::age>
    >>;

using PostTable = Table<"posts", Post,
    Columns<
        Column<"id", &Post::id, PrimaryKey<>>,
        Column<"author_id", &Post::author_id, ForeignKey<UserTable, &User::id>>,
        Column<"content", &Post::content>
    >>;

using AuthorTable = Table<"authors", Author,
    Columns<
        Column<"id", &Author::id, PrimaryKey<>>,
        Column<"name", &Author::name, NotNull>
    >>;

using BookTable = Table<"books", Book,
    Columns<
        Column<"id", &Book::id, PrimaryKey<>>,
        Column<"author_id", &Book::author_id, ForeignKey<AuthorTable, &Author::id>>,
        Column<"title", &Book::title, Unique>
    >>;
} // namespace tewi::tests

TEWI_REGISTER_TABLE(tewi::tests::User, tewi::tests::UserTable)
TEWI_REGISTER_TABLE(tewi::tests::Post, tewi::tests::PostTable)
TEWI_REGISTER_TABLE(tewi::tests::Author, tewi::tests::AuthorTable)
TEWI_REGISTER_TABLE(tewi::tests::Book, tewi::tests::BookTable)

// ---------------------------------------------------------
// 3. Tests
// ---------------------------------------------------------
namespace tewi::tests
{
TEST_CASE("ORM: Table Registration and Schema Generation", "[orm][schema]")
{
    // Verify that the table registry correctly maps User and Post to their tables
    using UserReg = detail::TableRegistry<User>;
    using PostReg = detail::TableRegistry<Post>;

    SECTION("User should be registered to UserTable")
    {
        STATIC_REQUIRE(std::is_same_v<UserReg::TableType, UserTable>);
    }
    SECTION("Post should be registered to PostTable")
    {
        STATIC_REQUIRE(std::is_same_v<PostReg::TableType, PostTable>);
    }
}

TEST_CASE("ORM Concepts: Type Classification Validation", "[orm][concepts]")
{
    SECTION("IsTable Concept")
    {
        // Valid tables should satisfy the concept
        STATIC_REQUIRE(tewi::detail::IsTable<AuthorTable>);
        STATIC_REQUIRE(tewi::detail::IsTable<BookTable>);

        // Plain structs or arbitrary types should fail the concept
        STATIC_REQUIRE_FALSE(tewi::detail::IsTable<Author>);
        STATIC_REQUIRE_FALSE(tewi::detail::IsTable<int>);
    }

    SECTION("HasRegisteredTable Concept")
    {
        STATIC_REQUIRE(tewi::detail::HasRegisteredTable<Author>);
        STATIC_REQUIRE(tewi::detail::HasRegisteredTable<Book>);

        // Unregistered structs should not be recognized
        STATIC_REQUIRE_FALSE(tewi::detail::HasRegisteredTable<UnregisteredEntity>);
    }

    SECTION("HomogeneousMemberPtrs Concept")
    {
        // Valid: All pointers belong to the same struct
        STATIC_REQUIRE(tewi::detail::HomogeneousMemberPtrs<&Author::id, &Author::name>);

        // Invalid: Mixed pointers from different structs
        STATIC_REQUIRE_FALSE(tewi::detail::HomogeneousMemberPtrs<&Author::id, &Book::title>);

        // Invalid: Empty pack (requires at least 1)
        STATIC_REQUIRE_FALSE(tewi::detail::HomogeneousMemberPtrs<>);
    }
}

TEST_CASE("ORM Metadata: Table and Column Reflection", "[orm][metadata]")
{
    SECTION("Compile-time Table Properties")
    {
        STATIC_REQUIRE(AuthorTable::TableName == "authors");
        STATIC_REQUIRE(AuthorTable::ColumnsCount == 2);

        STATIC_REQUIRE(BookTable::TableName == "books");
        STATIC_REQUIRE(BookTable::ColumnsCount == 3);
    }

    SECTION("Primary Key Resolution")
    {
        // find_pk() evaluates to string_view at compile time
        STATIC_REQUIRE(AuthorTable::pk_name == "id");
        STATIC_REQUIRE(BookTable::pk_name == "id");
    }

    SECTION("Column Name Resolution via Member Pointers")
    {
        // Compile-time lookups
        STATIC_REQUIRE(AuthorTable::column_name_for<&Author::name>() == "name");
        STATIC_REQUIRE(BookTable::column_name_for<&Book::author_id>() == "author_id");
        STATIC_REQUIRE(AuthorTable::column_name_for<&Author::id>() == "id");
        STATIC_REQUIRE(BookTable::column_name_for<&Book::title>() == "title");

        // Querying a member pointer not in the table should return an empty view
        STATIC_REQUIRE(AuthorTable::column_name_for<&Book::id>().empty());
    }
}

TEST_CASE("ORM Constraints: Foreign Key Detection", "[orm][constraints]")
{
    SECTION("Directional Foreign Key Resolution")
    {
        // BookTable has a FK pointing to AuthorTable
        STATIC_REQUIRE(tewi::detail::HasFkTo<BookTable, AuthorTable>);

        // AuthorTable does NOT have a FK pointing to BookTable
        STATIC_REQUIRE_FALSE(tewi::detail::HasFkTo<AuthorTable, BookTable>);
    }

    SECTION("Extracting Foreign Key Column Data")
    {
        // Isolate the column type that holds the FK
        using FkCol = detail::FkColumn<BookTable, AuthorTable>;

        // Ensure the correct column was found
        STATIC_REQUIRE(FkCol::ColumnName == "author_id");

        // Ensure the reference resolves back to the target table's correct column
        constexpr std::string_view ref_col =
            tewi::detail::fk_referenced_col_name<FkCol, AuthorTable>();
        STATIC_REQUIRE(ref_col == "id");
    }
}

TEST_CASE("ORM: Column Mapping and Constraints", "[orm][schema]")
{
    SECTION("UserTable has the correct column definitions")
    {
        STATIC_REQUIRE(UserTable::ColumnsCount == 3);
        STATIC_REQUIRE(std::tuple_element_t<0, UserTable::ColumnsTuple>::ColumnName == "id");
        STATIC_REQUIRE(std::tuple_element_t<1, UserTable::ColumnsTuple>::ColumnName == "username");
        STATIC_REQUIRE(std::tuple_element_t<2, UserTable::ColumnsTuple>::ColumnName == "age");
    }

    SECTION("PostTable has the correct column definitions")
    {
        STATIC_REQUIRE(PostTable::ColumnsCount == 3);
        STATIC_REQUIRE(std::tuple_element_t<0, PostTable::ColumnsTuple>::ColumnName == "id");
        STATIC_REQUIRE(std::tuple_element_t<1, PostTable::ColumnsTuple>::ColumnName == "author_id");
        STATIC_REQUIRE(std::tuple_element_t<2, PostTable::ColumnsTuple>::ColumnName == "content");
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
    auto raw = tewi::engine::InMemory();
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
    auto raw = tewi::engine::InMemory();
    tewi::OrmDatabase db(raw);
    tewi::createTablesIfNotExist<UserTable, PostTable>(raw);

    i64 author_id = db.repo<UserTable>().insert({0, "Writer1", 35});

    db.repo<PostTable>().insert({.id=1, .author_id=static_cast<int>(author_id), .content="Hello World"});
    db.repo<PostTable>().insert({.id=2, .author_id=static_cast<int>(author_id), .content="Second Post"});

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
