#include <catch2/catch_test_macros.hpp>

#include "Tables.h"

import tewi;
import std;

namespace tewi::tests
{
TEST_CASE("Primary keys: count", "[orm][schema][composite-pk]")
{
    SECTION("A simple table shall expose a count of one (1) primary key column")
    {
        STATIC_REQUIRE(SimpleEntityTable::primaryKeyCount == 1);
    }
    SECTION("A table shall expose the count of primary key columns in the composite key")
    {
        STATIC_REQUIRE(OrderItemTable::primaryKeyCount == 2);
        STATIC_REQUIRE(ThreeKeyCompositeTable::primaryKeyCount == 3);
    }
}

TEST_CASE("Primary keys: column flag", "[orm][schema][composite-pk]")
{
    SECTION("Each composite keys shall have the isPrimaryKey flag set")
    {
        STATIC_REQUIRE(OrderItemTable::ColumnOf<&OrderItem::order_id>::isPrimaryKey);
        STATIC_REQUIRE(OrderItemTable::ColumnOf<&OrderItem::item_number>::isPrimaryKey);
    }
    SECTION("Non-key columns shall not have the isPrimaryKey flag set")
    {
        STATIC_REQUIRE_FALSE(OrderItemTable::ColumnOf<&OrderItem::description>::isPrimaryKey);
        STATIC_REQUIRE_FALSE(OrderItemTable::ColumnOf<&OrderItem::quantity>::isPrimaryKey);
    }
}

TEST_CASE("Primary keys: tuple type", "[orm][schema][composite-pk]")
{
    SECTION("A table shall expose the type of its composite key as a tuple")
    {
        STATIC_REQUIRE(std::is_same_v<OrderItemTable::KeyType, std::tuple<i64, i32>>);
    }
    SECTION("Non-key columns shall not be included in the composite key type")
    {
        STATIC_REQUIRE_FALSE(std::is_same_v<OrderItemTable::KeyType, std::tuple<i32, i32, std::string>>);
    }
    SECTION("The tuple elements shall be in the same order of the table declaration")
    {
        using K1 = std::tuple_element_t<0, ThreeKeyCompositeTable::KeyTuple>;
        using K2 = std::tuple_element_t<1, ThreeKeyCompositeTable::KeyTuple>;
        using K3 = std::tuple_element_t<2, ThreeKeyCompositeTable::KeyTuple>;

        using T1 = std::tuple_element_t<0, ThreeKeyCompositeTable::KeyType>;
        using T2 = std::tuple_element_t<1, ThreeKeyCompositeTable::KeyType>;
        using T3 = std::tuple_element_t<2, ThreeKeyCompositeTable::KeyType>;

        STATIC_REQUIRE(std::is_same_v<K1::FieldType, T1>);
        STATIC_REQUIRE(std::is_same_v<K2::FieldType, T2>);
        STATIC_REQUIRE(std::is_same_v<K3::FieldType, T3>);

        STATIC_REQUIRE(std::is_same_v<ThreeKeyCompositeTable::KeyType, std::tuple<i32, i32, i64>>);
    }
}

TEST_CASE("Primary keys: auto-increment constraint", "[orm][schema][composite-pk]")
{
    SECTION("A table with an auto-increment primary key shall have only one (1) primary key")
    {
        STATIC_REQUIRE(SimpleEntityTable::hasAutoIncrementPk);
        STATIC_REQUIRE(SimpleEntityTable::primaryKeyCount == 1);
    }
    SECTION("A table with a composite key shall have no auto-increment keys")
    {
        STATIC_REQUIRE_FALSE(OrderItemTable::hasAutoIncrementPk);
        STATIC_REQUIRE_FALSE(ThreeKeyCompositeTable::hasAutoIncrementPk);
    }
}

TEST_CASE("Primary keys: columns tuple", "[orm][schema][composite-pk]")
{
    SECTION("The table shall expose a tuple of its primary key column types")
    {
        STATIC_REQUIRE(std::tuple_size<ThreeKeyCompositeTable::KeyTuple>::value == 3);
        STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<0, ThreeKeyCompositeTable::KeyTuple>, K1Col>);
        STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<1, ThreeKeyCompositeTable::KeyTuple>, K2Col>);
        STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<2, ThreeKeyCompositeTable::KeyTuple>, K3Col>);
    }
}

// =========================================================================
//  Runtime Tests (Hydration, Binding, Database Operations)
// =========================================================================

TEST_CASE("ORM Hydration: Populate structs from composite key results", "[orm][runtime][composite-pk]")
{
    auto raw = engine::InMemory();
    OrmDatabase db(raw);
    auto user_groups = db.repo<OrderItemTable>();
    user_groups.createTable();

    user_groups.insert({1, 10, "first", 1});
    user_groups.insert({2, 10, "second", 2});

    SECTION("Hydrate OrderItem from database row")
    {
        auto fetched = std::move(db.select<OrderItemTable>())
                           .where<&OrderItem::order_id>(1)
                           .where<&OrderItem::item_number>(10)
                           .firstOrDefault();

        REQUIRE(fetched.has_value());
        REQUIRE(fetched->order_id == 1);
        REQUIRE(fetched->item_number == 10);
        REQUIRE(fetched->description == "first");
        REQUIRE(fetched->quantity == 1);
    }

    SECTION("Hydrate multiple rows sequentially")
    {
        auto rows = db.select<OrderItemTable>()
                      .orderBy<&OrderItem::order_id>()
                      .toVector();

        REQUIRE(rows.size() == 2);
        REQUIRE(rows[0].order_id == 1);
        REQUIRE(rows[0].item_number == 10);
        REQUIRE(rows[0].description == "first");

        REQUIRE(rows[1].order_id == 2);
        REQUIRE(rows[1].item_number == 10);
        REQUIRE(rows[1].description == "second");
    }
}

TEST_CASE("ORM Binding: Bind composite key structs to statements", "[orm][runtime][composite-pk]")
{
    auto raw = engine::InMemory();
    OrmDatabase db(raw);
    auto user_groups = db.repo<OrderItemTable>();
    user_groups.createTable();

    SECTION("Bind and insert OrderItem with composite key")
    {
        OrderItem ug{5, 20, "serving n.4", 2};

        user_groups.insert(ug);

        auto check = db.select<OrderItemTable>()
                       .where<&OrderItem::order_id>(5)
                       .where<&OrderItem::item_number>(20)
                       .firstOrDefault();

        REQUIRE(check.has_value());
        REQUIRE(check->order_id == 5);
        REQUIRE(check->item_number == 20);
        REQUIRE(check->description == "serving n.4");
        REQUIRE(check->quantity == 2);
    }

    SECTION("Bind multiple composites")
    {
        OrderItem ug1{3, 15, "viewer"};
        OrderItem ug2{4, 15, "editor"};

        user_groups.insert(ug1);
        user_groups.insert(ug2);

        auto rows = db.select<OrderItemTable>()
                      .where<&OrderItem::item_number>(15)
                      .orderBy<&OrderItem::order_id>()
                      .toVector();

        REQUIRE(rows.size() == 2);
        REQUIRE(rows[0].order_id == 3);
        REQUIRE(rows[0].description == "viewer");
        REQUIRE(rows[1].order_id == 4);
        REQUIRE(rows[1].description == "editor");
    }
}

TEST_CASE("ORM Database: Full CRUD with composite keys", "[orm][runtime][composite-pk]")
{
    auto raw = engine::InMemory();
    OrmDatabase db(raw);
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
    auto raw = engine::InMemory();
    OrmDatabase db(raw);
    auto resources = db.repo<ThreeKeyCompositeTable>();
    resources.createTable();

    SECTION("Insert and query by three-column key")
    {
        ThreeKeyComposite resource{1, 5, 42, "Production Database"};

        resources.insert(resource);

        auto fetched = db.select<ThreeKeyCompositeTable>()
                         .where<&ThreeKeyComposite::key1>(1)
                         .where<&ThreeKeyComposite::key2>(5)
                         .where<&ThreeKeyComposite::key3>(42)
                         .firstOrDefault();

        REQUIRE(fetched.has_value());
        REQUIRE(fetched->key1 == 1);
        REQUIRE(fetched->key2 == 5);
        REQUIRE(fetched->key3 == 42);
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
                      .where<&ThreeKeyComposite::key1>(1)
                      .where<&ThreeKeyComposite::key2>(5)
                      .orderBy<&ThreeKeyComposite::key3>()
                      .toVector();

        REQUIRE(rows.size() == 2);
        REQUIRE(rows[0].key3 == 1);
        REQUIRE(rows[1].key3 == 2);
    }

    SECTION("Delete by composite key")
    {
        ThreeKeyComposite resource{1, 5, 42, "Production Database"};

        resources.insert(resource);

        REQUIRE(resources.count() == 1);

        resources.remove(1, 5, 42);

        REQUIRE(resources.count() == 0);
    }
}
} // namespace tewi::tests