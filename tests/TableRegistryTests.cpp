#include "tewi/RegisterTable.h"

#include <catch2/catch_test_macros.hpp>

import tewi;

namespace tewi::tests
{
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

using UserTable =
    Table<"users", User,
          Columns<Column<"id", &User::id, PrimaryKey<>>,
                  Column<"username", &User::username, NotNull, Unique>, Column<"age", &User::age>>>;

using PostTable =
    Table<"posts", Post,
          Columns<Column<"id", &Post::id, PrimaryKey<>>,
                  Column<"author_id", &Post::author_id, ForeignKey<UserTable, &User::id>>,
                  Column<"content", &Post::content>>>;
} // namespace tewi::tests

TEWI_REGISTER_TABLE(tewi::tests::User, tewi::tests::UserTable)

namespace tewi::tests
{
TEST_CASE("Table Registration", "[orm][registry]")
{
    // Verify that the table registry correctly maps User and Post to their tables
    using UserReg = detail::TableRegistry<User>;
    using PostReg = detail::TableRegistry<Post>;

    SECTION("User shall be registered to UserTable")
    {
        STATIC_REQUIRE(std::is_same_v<UserReg::TableType, UserTable>);
    }
    SECTION("HasRegisteredTable shall be true for User")
    {
        STATIC_REQUIRE(detail::HasRegisteredTable<User>);
    }
    SECTION("Post shall not be registered to PostTable")
    {
        STATIC_REQUIRE_FALSE(std::is_same_v<PostReg::TableType, PostTable>);
        STATIC_REQUIRE(std::is_same_v<PostReg::TableType, void>);
    }
    SECTION("HasRegisteredTable shall be false for Post")
    {
        STATIC_REQUIRE_FALSE(detail::HasRegisteredTable<Post>);
    }
}
} // namespace tewi::tests
