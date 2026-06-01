#include <catch2/catch_test_macros.hpp>

#include "Tables.h"

namespace tewi::tests
{
TEST_CASE("DDL: CREATE TABLE", "[orm][schema][composite-pk]")
{
    std::string ddl = OrderItemTable::create_table_sql(true);

    SECTION("The table creation DDL shall start with CREATE TABLE")
    {
        REQUIRE(ddl.find("CREATE TABLE IF NOT EXISTS order_items") == 0);
    }
    SECTION("The table creation DDL shall contain all column definitions")
    {
        REQUIRE(ddl.find("order_id INTEGER") != std::string::npos);
        REQUIRE(ddl.find("item_number INTEGER") != std::string::npos);
        REQUIRE(ddl.find("description TEXT") != std::string::npos);
        REQUIRE(ddl.find("quantity INTEGER") != std::string::npos);
    }
    SECTION("The table creation DDL shall not define primary keys at the column level")
    {
        REQUIRE(ddl.find("order_id INTEGER PRIMARY KEY") == std::string::npos);
        REQUIRE(ddl.find("item_number INTEGER PRIMARY KEY") == std::string::npos);
    }
    SECTION("The table creation DDL shall define the composite primary key at the table level")
    {
        REQUIRE(ddl.find("PRIMARY KEY (order_id, item_number)") != std::string::npos);
    }
    SECTION("The table creation DDL shall support NOT NULL constraints")
    {
        std::string ddlNotNull = ThreeKeyCompositeTable::create_table_sql(true);

        REQUIRE(ddlNotNull.find("name TEXT NOT NULL") != std::string::npos);
    }
    SECTION("The table creation DDL shall support auto-increment primary key as table constraint")
    {
        std::string ddlAutoincrement = SimpleEntityTable::create_table_sql(true);

        REQUIRE(ddlAutoincrement.find("PRIMARY KEY (id AUTOINCREMENT)") != std::string::npos);
        REQUIRE(ddlAutoincrement.find("id INTEGER PRIMARY KEY") == std::string::npos);
    }
}
}