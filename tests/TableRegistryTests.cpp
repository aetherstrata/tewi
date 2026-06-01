#include <catch2/catch_test_macros.hpp>

#include "Tables.h"

import tewi;

namespace tewi::tests
{
TEST_CASE("Table Registry", "[orm][table][registry]")
{
    // Verify that the table registry correctly maps User and Post
    using SimpleReg = detail::TableRegistry<SimpleEntity>;
    using NonReg = detail::TableRegistry<UnregisteredEntity>;

    SECTION("User shall be registered to UserTable")
    {
        STATIC_REQUIRE(std::is_same_v<SimpleReg::TableType, SimpleEntityTable>);
    }
    SECTION("HasRegisteredTable shall be true for User")
    {
        STATIC_REQUIRE(detail::HasRegisteredTable<User>);
    }
    SECTION("Post shall not be registered to PostTable")
    {
        STATIC_REQUIRE(std::is_same_v<NonReg::TableType, void>);
    }
    SECTION("HasRegisteredTable shall be false for Post")
    {
        STATIC_REQUIRE_FALSE(detail::HasRegisteredTable<UnregisteredEntity>);
    }
}
} // namespace tewi::tests
