#ifndef TEWI_TABLES_H
#define TEWI_TABLES_H

import tewi;
import std;

namespace tewi::tests
{
// ---------------------------------------------------------
//  Define plain structs
// ---------------------------------------------------------
struct User
{
    i32 id = 0;
    std::string username;
    std::optional<i32> age;
};

struct Post
{
    i32 id        = 0;
    i32 author_id = 0;
    std::string content;
};

/// OrderItem: order items with composite key (order_id + item_number)
struct OrderItem
{
    i64 order_id   = 0;
    i32 item_number = 0;
    std::string description;
    i32 quantity   = 0;
};

/// CompositePkTable: three-column composite key
struct ThreeKeyComposite
{
    i32 key1  = 0;
    i32 key2  = 0;
    i64 key3 = 0;
    std::string name;
};

/// SingleColumnTable for reference
struct SimpleEntity
{
    i32 id = 0;
    std::string value;
};

/// Plain old struct
struct UnregisteredEntity
{
    i32 dummy;
};

// ---------------------------------------------------------
//  Map ORM Tables
// ---------------------------------------------------------

using UserIdColumn = Column<"id", &User::id, PrimaryKey<>>;
using UserNameColumn = Column<"username", &User::username, NotNull, Unique>;
using AgeColumn = Column<"age", &User::age>;

using UserIndex = UniqueIndex<"idx_username", &User::username>;

using UserTable = Table<"users", User,
    Columns<UserIdColumn, UserNameColumn, AgeColumn>,
    Indexes<UserIndex>>;

using PostTable = Table<"posts", Post,
    Columns<
        Column<"id", &Post::id, PrimaryKey<>>,
        Column<"author_id", &Post::author_id, ForeignKey<UserTable, &User::id>>,
        Column<"content", &Post::content, NotNull>
    >>;

/// Two-column composite primary key (order_id, item_number)
using OrderItemTable = Table<"order_items", OrderItem,
    Columns<
        Column<"order_id", &OrderItem::order_id, PrimaryKey<false>>,
        Column<"item_number", &OrderItem::item_number, PrimaryKey<false>>,
        Column<"description", &OrderItem::description>,
        Column<"quantity", &OrderItem::quantity>
    >>;

using K1Col = Column<"key1", &ThreeKeyComposite::key1, PrimaryKey<false>>;
using K2Col = Column<"key2", &ThreeKeyComposite::key2, PrimaryKey<false>>;
using K3Col = Column<"key3", &ThreeKeyComposite::key3, PrimaryKey<false>>;

/// Three-column composite primary key (tenant_id, region_id, resource_id)
using ThreeKeyCompositeTable = Table<"three_key_composite", ThreeKeyComposite,
    Columns<
        K1Col, K2Col, K3Col,
        Column<"name", &ThreeKeyComposite::name, NotNull>
    >>;

/// Reference table with single PrimaryKey
using SimpleEntityTable = Table<"simple_entities", SimpleEntity,
    Columns<
        Column<"id", &SimpleEntity::id, PrimaryKey<>>,
        Column<"value", &SimpleEntity::value>
    >>;
} // namespace tewi::tests

#include "tewi/RegisterTable.h"

TEWI_REGISTER_TABLE(tewi::tests::UserTable)
TEWI_REGISTER_TABLE(tewi::tests::OrderItemTable)
TEWI_REGISTER_TABLE(tewi::tests::PostTable)
TEWI_REGISTER_TABLE(tewi::tests::ThreeKeyCompositeTable)
TEWI_REGISTER_TABLE(tewi::tests::SimpleEntityTable)

#endif // TEWI_TABLES_H
