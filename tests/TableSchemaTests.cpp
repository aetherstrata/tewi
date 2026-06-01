#include <catch2/catch_test_macros.hpp>

import tewi;
import std;

namespace tewi::tests
{
// =========================================================================
//  Test structs with composite primary keys
// =========================================================================

/// UserGroup: many-to-many relationship (user_id + group_id form composite PK)
struct UserGroup
{
    int user_id    = 0;
    int group_id   = 0;
    std::string role;
};

/// OrderItem: order items with composite key (order_id + item_number)
struct OrderItem
{
    int order_id   = 0;
    int item_number = 0;
    std::string description;
    int quantity   = 0;
};

/// CompositePkTable: three-column composite key
struct ThreeKeyComposite
{
    int tenant_id  = 0;
    int region_id  = 0;
    int resource_id = 0;
    std::string name;
};

/// SingleColumnTable for reference
struct SimpleEntity
{
    int id = 0;
    std::string value;
};

// =========================================================================
//  Table Definitions
// =========================================================================

/// Two-column composite primary key (user_id, group_id)
using UserGroupTable = Table<"user_groups", UserGroup,
    Columns<
        Column<"user_id", &UserGroup::user_id, PrimaryKey<false>>,
        Column<"group_id", &UserGroup::group_id, PrimaryKey<false>>,
        Column<"role", &UserGroup::role>
    >
>;

/// Two-column composite primary key (order_id, item_number)
using OrderItemTable = Table<"order_items", OrderItem,
    Columns<
        Column<"order_id", &OrderItem::order_id, PrimaryKey<false>>,
        Column<"item_number", &OrderItem::item_number, PrimaryKey<false>>,
        Column<"description", &OrderItem::description>,
        Column<"quantity", &OrderItem::quantity>
    >
>;

/// Three-column composite primary key (tenant_id, region_id, resource_id)
using ThreeKeyCompositeTable = Table<"three_key_composite", ThreeKeyComposite,
    Columns<
        Column<"tenant_id", &ThreeKeyComposite::tenant_id, PrimaryKey<false>>,
        Column<"region_id", &ThreeKeyComposite::region_id, PrimaryKey<false>>,
        Column<"resource_id", &ThreeKeyComposite::resource_id, PrimaryKey<false>>,
        Column<"name", &ThreeKeyComposite::name, NotNull>
    >
>;

/// Reference table with single PrimaryKey
using SimpleEntityTable = Table<"simple_entities", SimpleEntity,
    Columns<
        Column<"id", &SimpleEntity::id, PrimaryKey<>>,
        Column<"value", &SimpleEntity::value>
    >
>;

} // namespace tewi::tests

// =========================================================================
//  DDL and Compile-time Tests
// =========================================================================

TEST_CASE("ORM Table Schema: Composite Primary Keys", "[orm][schema][composite-pk]")
{
    using namespace tewi::tests;

    SECTION("Two-column composite key - table name and column count")
    {
        STATIC_REQUIRE(UserGroupTable::TableName == "user_groups");
        STATIC_REQUIRE(UserGroupTable::ColumnsCount == 3);
    }

    SECTION("Two-column composite key - column names")
    {
        using Col0 = std::tuple_element_t<0, UserGroupTable::ColumnsTuple>;
        using Col1 = std::tuple_element_t<1, UserGroupTable::ColumnsTuple>;
        using Col2 = std::tuple_element_t<2, UserGroupTable::ColumnsTuple>;

        STATIC_REQUIRE(Col0::ColumnName == "user_id");
        STATIC_REQUIRE(Col1::ColumnName == "group_id");
        STATIC_REQUIRE(Col2::ColumnName == "role");

        STATIC_REQUIRE(Col0::IsPrimaryKey);
        STATIC_REQUIRE(Col1::IsPrimaryKey);
    }

    SECTION("Three-column composite key - table properties")
    {
        STATIC_REQUIRE(ThreeKeyCompositeTable::TableName == "three_key_composite");
        STATIC_REQUIRE(ThreeKeyCompositeTable::ColumnsCount == 4);

        using Col0 = std::tuple_element_t<0, ThreeKeyCompositeTable::ColumnsTuple>;
        using Col1 = std::tuple_element_t<1, ThreeKeyCompositeTable::ColumnsTuple>;
        using Col2 = std::tuple_element_t<2, ThreeKeyCompositeTable::ColumnsTuple>;

        STATIC_REQUIRE(Col0::IsPrimaryKey);
        STATIC_REQUIRE(Col1::IsPrimaryKey);
        STATIC_REQUIRE(Col2::IsPrimaryKey);
    }
}

TEST_CASE("ORM Constraints: Multiple PrimaryKey Columns Detection", "[orm][schema][composite-pk]")
{
    using namespace tewi::tests;

    SECTION("UserGroupTable - both columns are marked as PrimaryKey")
    {
        using Col0 = std::tuple_element_t<0, UserGroupTable::ColumnsTuple>;
        using Col1 = std::tuple_element_t<1, UserGroupTable::ColumnsTuple>;
        using Col2 = std::tuple_element_t<2, UserGroupTable::ColumnsTuple>;

        STATIC_REQUIRE(Col0::IsPrimaryKey);
        STATIC_REQUIRE(Col1::IsPrimaryKey);
        STATIC_REQUIRE_FALSE(Col2::IsPrimaryKey);
    }

    SECTION("OrderItemTable - first two columns are primary keys")
    {
        using Col0 = std::tuple_element_t<0, OrderItemTable::ColumnsTuple>;
        using Col1 = std::tuple_element_t<1, OrderItemTable::ColumnsTuple>;
        using Col2 = std::tuple_element_t<2, OrderItemTable::ColumnsTuple>;
        using Col3 = std::tuple_element_t<3, OrderItemTable::ColumnsTuple>;

        STATIC_REQUIRE(Col0::IsPrimaryKey);
        STATIC_REQUIRE(Col1::IsPrimaryKey);
        STATIC_REQUIRE_FALSE(Col2::IsPrimaryKey);
        STATIC_REQUIRE_FALSE(Col3::IsPrimaryKey);
    }

    SECTION("ThreeKeyCompositeTable - all three columns are primary keys")
    {
        using Col0 = std::tuple_element_t<0, ThreeKeyCompositeTable::ColumnsTuple>;
        using Col1 = std::tuple_element_t<1, ThreeKeyCompositeTable::ColumnsTuple>;
        using Col2 = std::tuple_element_t<2, ThreeKeyCompositeTable::ColumnsTuple>;
        using Col3 = std::tuple_element_t<3, ThreeKeyCompositeTable::ColumnsTuple>;

        STATIC_REQUIRE(Col0::IsPrimaryKey);
        STATIC_REQUIRE(Col1::IsPrimaryKey);
        STATIC_REQUIRE(Col2::IsPrimaryKey);
        STATIC_REQUIRE_FALSE(Col3::IsPrimaryKey);
    }
}

TEST_CASE("ORM Metadata: Column Name Resolution with Composite Keys", "[orm][schema][composite-pk]")
{
    using namespace tewi::tests;

    SECTION("UserGroupTable member pointer lookups")
    {
        STATIC_REQUIRE(UserGroupTable::column_name_for<&UserGroup::user_id>() == "user_id");
        STATIC_REQUIRE(UserGroupTable::column_name_for<&UserGroup::group_id>() == "group_id");
        STATIC_REQUIRE(UserGroupTable::column_name_for<&UserGroup::role>() == "role");
    }

    SECTION("OrderItemTable member pointer lookups")
    {
        STATIC_REQUIRE(OrderItemTable::column_name_for<&OrderItem::order_id>() == "order_id");
        STATIC_REQUIRE(OrderItemTable::column_name_for<&OrderItem::item_number>() == "item_number");
        STATIC_REQUIRE(OrderItemTable::column_name_for<&OrderItem::description>() == "description");
        STATIC_REQUIRE(OrderItemTable::column_name_for<&OrderItem::quantity>() == "quantity");
    }

    SECTION("ThreeKeyCompositeTable member pointer lookups")
    {
        STATIC_REQUIRE(ThreeKeyCompositeTable::column_name_for<&ThreeKeyComposite::tenant_id>()
                       == "tenant_id");
        STATIC_REQUIRE(ThreeKeyCompositeTable::column_name_for<&ThreeKeyComposite::region_id>()
                       == "region_id");
        STATIC_REQUIRE(
            ThreeKeyCompositeTable::column_name_for<&ThreeKeyComposite::resource_id>()
            == "resource_id");
        STATIC_REQUIRE(ThreeKeyCompositeTable::column_name_for<&ThreeKeyComposite::name>()
                       == "name");
    }
}

TEST_CASE("ORM DDL: CREATE TABLE with Composite Primary Keys", "[orm][schema][composite-pk]")
{
    using namespace tewi::tests;

    SECTION("UserGroupTable DDL generation")
    {
        std::string ddl = UserGroupTable::create_table_sql(true);

        // Must contain the table name
        REQUIRE(ddl.find("CREATE TABLE IF NOT EXISTS user_groups") != std::string::npos);

        // Must have all three columns
        REQUIRE(ddl.find("user_id") != std::string::npos);
        REQUIRE(ddl.find("group_id") != std::string::npos);
        REQUIRE(ddl.find("role") != std::string::npos);

        // The primary key must be emitted once as a table constraint.
        REQUIRE(ddl.find("user_id INTEGER PRIMARY KEY") == std::string::npos);
        REQUIRE(ddl.find("group_id INTEGER PRIMARY KEY") == std::string::npos);
        REQUIRE(ddl.find("PRIMARY KEY (user_id, group_id)") != std::string::npos);
    }

    SECTION("OrderItemTable DDL generation")
    {
        std::string ddl = OrderItemTable::create_table_sql(false);

        // Must NOT have IF NOT EXISTS
        REQUIRE(ddl.find("IF NOT EXISTS") == std::string::npos);

        // Must contain the table name and all columns
        REQUIRE(ddl.find("CREATE TABLE order_items") != std::string::npos);
        REQUIRE(ddl.find("order_id") != std::string::npos);
        REQUIRE(ddl.find("item_number") != std::string::npos);

        // Verify the key is emitted once at table level.
        REQUIRE(ddl.find("PRIMARY KEY (order_id, item_number)") != std::string::npos);
    }

    SECTION("ThreeKeyCompositeTable DDL generation")
    {
        std::string ddl = ThreeKeyCompositeTable::create_table_sql(true);

        // All three key columns should be present.
        REQUIRE(ddl.find("tenant_id") != std::string::npos);
        REQUIRE(ddl.find("region_id") != std::string::npos);
        REQUIRE(ddl.find("resource_id") != std::string::npos);

        // NotNull constraint on name.
        REQUIRE(ddl.find("NOT NULL") != std::string::npos);

        REQUIRE(ddl.find("PRIMARY KEY (tenant_id, region_id, resource_id)") != std::string::npos);
        REQUIRE(ddl.find("tenant_id INTEGER PRIMARY KEY") == std::string::npos);
        REQUIRE(ddl.find("region_id INTEGER PRIMARY KEY") == std::string::npos);
        REQUIRE(ddl.find("resource_id INTEGER PRIMARY KEY") == std::string::npos);
    }

    SECTION("SimpleEntityTable DDL generation")
    {
        std::string ddl = SimpleEntityTable::create_table_sql(true);

        REQUIRE(ddl.find("PRIMARY KEY (id AUTOINCREMENT)") != std::string::npos);
        REQUIRE(ddl.find("id INTEGER PRIMARY KEY") == std::string::npos);
    }
}

TEST_CASE("ORM Column List: column_list() with Composite Keys", "[orm][schema][composite-pk]")
{
    using namespace tewi::tests;

    SECTION("UserGroupTable column list without prefix")
    {
        std::string cols = UserGroupTable::column_list();
        REQUIRE(cols == "user_id, group_id, role");
    }

    SECTION("UserGroupTable column list with prefix")
    {
        std::string cols = UserGroupTable::column_list("ug");
        REQUIRE(cols == "ug.user_id, ug.group_id, ug.role");
    }

    SECTION("OrderItemTable column list with prefix")
    {
        std::string cols = OrderItemTable::column_list("oi");
        REQUIRE(cols == "oi.order_id, oi.item_number, oi.description, oi.quantity");
    }

    SECTION("ThreeKeyCompositeTable column list")
    {
        std::string cols = ThreeKeyCompositeTable::column_list("tkc");
        REQUIRE(cols == "tkc.tenant_id, tkc.region_id, tkc.resource_id, tkc.name");
    }
}

// =========================================================================
//  Runtime Tests (Hydration, Binding, Database Operations)
// =========================================================================

TEST_CASE("ORM Hydration: Populate structs from composite key results", "[orm][runtime][composite-pk]")
{
    using namespace tewi::tests;

    auto raw = tewi::engine::InMemory();
    tewi::OrmDatabase db(raw);
    auto user_groups = db.repo<UserGroupTable>();
    user_groups.createTable();

    user_groups.insert({1, 10, "admin"});
    user_groups.insert({2, 10, "member"});

    SECTION("Hydrate UserGroup from database row")
    {
        auto fetched = std::move(db.select<UserGroupTable>())
                           .where<&UserGroup::user_id>(1)
                           .where<&UserGroup::group_id>(10)
                           .firstOrDefault();

        REQUIRE(fetched.has_value());
        REQUIRE(fetched->user_id == 1);
        REQUIRE(fetched->group_id == 10);
        REQUIRE(fetched->role == "admin");
    }

    SECTION("Hydrate multiple rows sequentially")
    {
        auto rows = std::move(db.select<UserGroupTable>())
                        .orderBy<&UserGroup::user_id>()
                        .toVector();

        REQUIRE(rows.size() == 2);
        REQUIRE(rows[0].user_id == 1);
        REQUIRE(rows[0].group_id == 10);
        REQUIRE(rows[0].role == "admin");

        REQUIRE(rows[1].user_id == 2);
        REQUIRE(rows[1].group_id == 10);
        REQUIRE(rows[1].role == "member");
    }
}

TEST_CASE("ORM Binding: Bind composite key structs to statements", "[orm][runtime][composite-pk]")
{
    using namespace tewi::tests;

    auto raw = tewi::engine::InMemory();
    tewi::OrmDatabase db(raw);
    auto user_groups = db.repo<UserGroupTable>();
    user_groups.createTable();

    SECTION("Bind and insert UserGroup with composite key")
    {
        UserGroup ug{5, 20, "supervisor"};

        user_groups.insert(ug);

        auto check = db.select<UserGroupTable>()
					   .where<&UserGroup::user_id>(5)
					   .where<&UserGroup::group_id>(20)
					   .firstOrDefault();

        REQUIRE(check.has_value());
        REQUIRE(check->user_id == 5);
        REQUIRE(check->group_id == 20);
        REQUIRE(check->role == "supervisor");
    }

    SECTION("Bind multiple composites")
    {
        UserGroup ug1{3, 15, "viewer"};
        UserGroup ug2{4, 15, "editor"};

        user_groups.insert(ug1);
        user_groups.insert(ug2);

        auto rows = db.select<UserGroupTable>()
					  .where<&UserGroup::group_id>(15)
					  .orderBy<&UserGroup::user_id>()
					  .toVector();

        REQUIRE(rows.size() == 2);
        REQUIRE(rows[0].user_id == 3);
        REQUIRE(rows[0].role == "viewer");
        REQUIRE(rows[1].user_id == 4);
        REQUIRE(rows[1].role == "editor");
    }
}

TEST_CASE("ORM Database: Full CRUD with composite keys", "[orm][runtime][composite-pk]")
{
    using namespace tewi::tests;

    auto raw = tewi::engine::InMemory();
    tewi::OrmDatabase db(raw);
    auto order_items = db.repo<OrderItemTable>();

    order_items.createTable();

    SECTION("Insert order items with composite key")
    {
        OrderItem item1{100, 1, "Widget", 5};
        OrderItem item2{100, 2, "Gadget", 3};
        OrderItem item3{101, 1, "Widget", 2};

        order_items.insert(item1);
        order_items.insert(item2);
        order_items.insert(item3);

        REQUIRE(order_items.count() == 3);
    }

    SECTION("Query by composite key (order_id + item_number)")
    {
        OrderItem item{200, 5, "Doohickey", 7};
        order_items.insert(item);

        auto fetched = std::move(db.select<OrderItemTable>())
                           .where<&OrderItem::order_id>(200)
                           .where<&OrderItem::item_number>(5)
                           .firstOrDefault();

        REQUIRE(fetched.has_value());
        REQUIRE(fetched->order_id == 200);
        REQUIRE(fetched->item_number == 5);
        REQUIRE(fetched->description == "Doohickey");
        REQUIRE(fetched->quantity == 7);
    }

    SECTION("Query all items for a specific order")
    {
        OrderItem item1{300, 1, "Item A", 2};
        OrderItem item2{300, 2, "Item B", 4};
        OrderItem item3{301, 1, "Item C", 1};

        order_items.insert(item1);
        order_items.insert(item2);
        order_items.insert(item3);

        auto rows = db.select<OrderItemTable>()
                      .where<&OrderItem::order_id>(300)
                      .orderBy<&OrderItem::item_number>()
                      .toVector();

        REQUIRE(rows.size() == 2);
        REQUIRE(rows[0].item_number == 1);
        REQUIRE(rows[0].description == "Item A");
        REQUIRE(rows[1].item_number == 2);
        REQUIRE(rows[1].description == "Item B");
    }
}

TEST_CASE("ORM Three-Column Composite Keys", "[orm][runtime][composite-pk]")
{
    using namespace tewi::tests;

    auto raw = tewi::engine::InMemory();
    tewi::OrmDatabase db(raw);
    auto resources = db.repo<ThreeKeyCompositeTable>();
    resources.createTable();

    SECTION("Insert and query by three-column key")
    {
        ThreeKeyComposite resource{1, 5, 42, "Production Database"};

        resources.insert(resource);

        auto fetched = db.select<ThreeKeyCompositeTable>()
                         .where<&ThreeKeyComposite::tenant_id>(1)
                         .where<&ThreeKeyComposite::region_id>(5)
                         .where<&ThreeKeyComposite::resource_id>(42)
                         .firstOrDefault();

        REQUIRE(fetched.has_value());
        REQUIRE(fetched->tenant_id == 1);
        REQUIRE(fetched->region_id == 5);
        REQUIRE(fetched->resource_id == 42);
        REQUIRE(fetched->name == "Production Database");
    }

    SECTION("Query by partial composite key")
    {
        ThreeKeyComposite r1{1, 5, 1, "Resource 1"};
        ThreeKeyComposite r2{1, 5, 2, "Resource 2"};
        ThreeKeyComposite r3{1, 6, 1, "Resource 3"};

        resources.insert(r1);
        resources.insert(r2);
        resources.insert(r3);

        auto rows = db.select<ThreeKeyCompositeTable>()
                      .where<&ThreeKeyComposite::tenant_id>(1)
                      .where<&ThreeKeyComposite::region_id>(5)
                      .orderBy<&ThreeKeyComposite::resource_id>()
                      .toVector();

        REQUIRE(rows.size() == 2);
        REQUIRE(rows[0].resource_id == 1);
        REQUIRE(rows[1].resource_id == 2);
    }

    SECTION("Delete by composite key")
    {
        ThreeKeyComposite resource{1, 5, 42, "Production Database"};

        resources.insert(resource);

        REQUIRE(resources.count() == 1);

        resources.remove(resource);

    }
}

