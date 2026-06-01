#include <catch2/catch_test_macros.hpp>

import tewi;
import std;

using namespace tewi;

struct User
{
    int id;
    int age;
    std::string name;
};

using IdColumn = Column<"id", &User::id>;

using AgeColumn = Column<"age", &User::age>;

using NameColumn = Column<"name", &User::name>;

using DuplicateNameColumn = Column<"id", &User::age>;

using DuplicateMemberColumn = Column<"other_id", &User::id>;

TEST_CASE("UniqueColumnNames shall accept distinct names", "[orm][schema][column]")
{
    STATIC_REQUIRE(detail::UniqueColumnNames<IdColumn, AgeColumn, NameColumn>);
}

TEST_CASE("UniqueColumnNames shall reject duplicates", "[orm][schema][column]")
{
    STATIC_REQUIRE_FALSE(detail::UniqueColumnNames<IdColumn, DuplicateNameColumn>);
}

TEST_CASE("UniqueMemberPointers shall accept distinct members", "[orm][schema][column]")
{
    STATIC_REQUIRE(detail::UniqueMemberPointers<IdColumn, AgeColumn, NameColumn>);
}

TEST_CASE("UniqueMemberPointers shall reject duplicate members", "[orm][schema][column]")
{
    STATIC_REQUIRE_FALSE(detail::UniqueMemberPointers<IdColumn, DuplicateMemberColumn>);
}

TEST_CASE("UniqueMemberPointers shall handle different member pointer types", "[orm][schema][column]")
{
    STATIC_REQUIRE(detail::UniqueMemberPointers<IdColumn, NameColumn>);
}
