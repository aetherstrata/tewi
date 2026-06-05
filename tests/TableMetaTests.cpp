#include <catch2/catch_test_macros.hpp>

#include "Tables.h"

import tewi;
import std;

namespace tewi::tests
{
TEST_CASE("Table Metadata", "[orm][table][schema]")
{
    SECTION("A table shall expose its name in the database")
    {
        STATIC_REQUIRE(UserTable::tableName == "users");
    }
    SECTION("A table shall expose its column count")
    {
        STATIC_REQUIRE(UserTable::columnsCount == 3);
    }
    SECTION("A table shall expose a tuple of its column types")
    {
        STATIC_REQUIRE(std::tuple_size<UserTable::ColumnsTuple>::value == 3);
        STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<0, UserTable::ColumnsTuple>, UserIdColumn>);
        STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<1, UserTable::ColumnsTuple>, UserNameColumn>);
        STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<2, UserTable::ColumnsTuple>, AgeColumn>);
    }
    SECTION("Querying a member pointer shall return its column type")
    {
        STATIC_REQUIRE(std::is_same_v<UserTable::ColumnOf<&User::id>, UserIdColumn>);
        STATIC_REQUIRE(std::is_same_v<UserTable::ColumnOf<&User::username>, UserNameColumn>);
        STATIC_REQUIRE(std::is_same_v<UserTable::ColumnOf<&User::age>, AgeColumn>);
    }
    SECTION("The table shall be able to associate a member pointer to its column name")
    {
        STATIC_REQUIRE(UserTable::ColumnOf<&User::id>::columnName == "id");
        STATIC_REQUIRE(UserTable::ColumnOf<&User::username>::columnName == "username");
        STATIC_REQUIRE(UserTable::ColumnOf<&User::age>::columnName == "age");
    }
    SECTION("Querying a member pointer not in the table shall return void")
    {
        STATIC_REQUIRE(std::is_same_v<UserTable::ColumnOf<&Post::id>, void>);
    }
    SECTION("The table shall expose its index count")
    {
        STATIC_REQUIRE(UserTable::indexCount == 1);
    }
    SECTION("The table shall expose a tuple of its index types")
    {
        STATIC_REQUIRE(std::tuple_size<UserTable::IndicesTuple>::value == 1);
        STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<0, UserTable::IndicesTuple>, UserIndex>);
    }
}
} // namespace tewi::tests
