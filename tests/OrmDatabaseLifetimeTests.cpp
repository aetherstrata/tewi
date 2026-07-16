#include <catch2/catch_test_macros.hpp>

#include "Tables.h"

// ============================================================================
//  OrmDatabase hands out borrows of its connection: Repository and BasicQuery
//  both store a SqliteConnection& into its _db member. Anything built from a
//  temporary database therefore dangles the moment the full-expression ends,
//  so every accessor that borrows constrains its explicit object parameter:
//
//      template <ITable TableType, typename Self>
//      requires std::is_lvalue_reference_v<Self>
//      auto select(this Self&& self)
//
//  Three things make these assertions work, and each is easy to get wrong.
//
//  1. The requires-expression must sit inside a template. A non-template one is
//     checked eagerly, so an invalid expression in it is a hard compile error
//     rather than `false` - even a plainly bogus member name. Only
//     *substitution* of template arguments turns an invalid expression into an
//     unsatisfied requirement. Each call below therefore lives in a class
//     template parameterised on the object type. (GCC and Clang agree; this is
//     the standard's behaviour, not a compiler quirk.)
//
//  2. The value category is carried by declval<Db>(), not by a requires
//     parameter. `requires(Db db)` with Db = OrmDatabase&& would name an rvalue
//     reference - and a named rvalue reference is an *lvalue*, so the check
//     would silently test the wrong thing and pass for the wrong reason.
//
//  3. A bare negative proves nothing: `!Can<X>` is satisfied by ANY ill-formed
//     expression, so a typo'd name would keep it green forever. BorrowGuarded
//     below fixes that structurally by requiring the acceptances alongside the
//     rejections - a typo falsifies the positive half and fails the assertion.
//
//  The calls cannot be condensed into one parameterised on the member, because
//  what varies is the whole call expression, not just the name: the overloads
//  differ in template-argument kind and arity (a type, an NTTP pack, none at
//  all). A member pointer could not express it either - with deducing this,
//  &OrmDatabase::select<...> is a plain function pointer, and its Self cannot
//  be left to deduce, so the value category under test would have to be baked
//  into the pointer itself.
// ============================================================================

namespace tewi::tests
{
namespace
{
// One trait per call. Concepts cannot be template-template arguments, so these
// are class templates rather than concepts. Db is the object expression's type:
// OrmDatabase& for an lvalue call, OrmDatabase for an rvalue one.
template <typename Db>
struct CanSelectRow
    : std::bool_constant<requires { std::declval<Db>().template select<User>(); }> {};

template <typename Db>
struct CanSelectTable
    : std::bool_constant<requires { std::declval<Db>().template select<UserTable>(); }> {};

template <typename Db>
struct CanSelectProjection
    : std::bool_constant<requires {
          std::declval<Db>().template select<&User::id, &User::username>();
      }> {};

template <typename Db>
struct CanRepo
    : std::bool_constant<requires { std::declval<Db>().template repo<UserTable>(); }> {};

template <typename Db>
struct CanCount
    : std::bool_constant<requires { std::declval<Db>().template count<User>(); }> {};

template <typename Db>
struct CanBeginTransaction
    : std::bool_constant<requires { std::declval<Db>().beginTransaction(); }> {};

template <typename Db>
struct CanRawAccess
    : std::bool_constant<requires { std::declval<Db>().rawAccess(); }> {};

template <typename Db>
struct CanCreateTable
    : std::bool_constant<requires { std::declval<Db>().template createTable<UserTable>(); }> {};

/// An accessor is borrow-guarded when it is callable on either lvalue form and
/// on neither rvalue form. Stating both halves together is deliberate: the
/// acceptances are what prove the rejections are down to the value category
/// rather than to a malformed expression.
template <template <typename> class Accessor>
concept BorrowGuarded =  Accessor<OrmDatabase&>::value
                     &&  Accessor<const OrmDatabase&>::value
                     && !Accessor<OrmDatabase>::value
                     && !Accessor<const OrmDatabase>::value;
} // namespace

TEST_CASE("OrmDatabase: every borrowing accessor rejects rvalues", "[orm][lifetime]")
{
    STATIC_REQUIRE(BorrowGuarded<CanSelectRow>);
    STATIC_REQUIRE(BorrowGuarded<CanSelectTable>);
    STATIC_REQUIRE(BorrowGuarded<CanSelectProjection>);
    STATIC_REQUIRE(BorrowGuarded<CanRepo>);
    STATIC_REQUIRE(BorrowGuarded<CanCount>);
    STATIC_REQUIRE(BorrowGuarded<CanBeginTransaction>);
    STATIC_REQUIRE(BorrowGuarded<CanRawAccess>);
}

TEST_CASE("OrmDatabase: const lvalues are still accepted", "[orm][lifetime]")
{
    // Folded into BorrowGuarded above, but worth stating on its own: the
    // constraint is is_lvalue_reference_v<Self>, not "non-const lvalue". Self
    // deduces to const OrmDatabase&, which is still an lvalue reference, and
    // the accessors reach a non-const connection through the mutable _db.
    STATIC_REQUIRE(CanSelectRow<const OrmDatabase&>::value);
    STATIC_REQUIRE(CanRepo<const OrmDatabase&>::value);
    STATIC_REQUIRE(CanCount<const OrmDatabase&>::value);
}

TEST_CASE("OrmDatabase: the call shape this prevents", "[orm][lifetime]")
{
    // The rejections above are what stop the line someone would actually write
    // by accident:
    //
    //     auto q = tewi::InMemory().select<User>();   // ill-formed
    //
    // The query would hold a SqliteConnection& into a database destroyed at the
    // end of the full-expression. It cannot be written out here - naming it
    // outside a template is a hard error, per point 1 above - but InMemory()
    // returns a prvalue, so it deduces Self exactly as OrmDatabase does.
    STATIC_REQUIRE(std::is_same_v<decltype(InMemory()), OrmDatabase>);
    STATIC_REQUIRE(std::is_same_v<decltype(Open(std::declval<std::filesystem::path>())),
                                  OrmDatabase>);
    STATIC_REQUIRE(!CanSelectRow<decltype(InMemory())>::value);
    STATIC_REQUIRE(!CanRepo<decltype(InMemory())>::value);
}

TEST_CASE("OrmDatabase: ownership semantics", "[orm][lifetime]")
{
    // Moving is allowed: a factory that builds a database and returns it by
    // value borrows nothing yet, so the move is safe. Deleting it would break
    // `OrmDatabase makeDb() { OrmDatabase db = InMemory(); ...; return db; }`,
    // which needs an accessible move constructor even when NRVO elides it.
    STATIC_REQUIRE(std::is_move_constructible_v<OrmDatabase>);
    STATIC_REQUIRE(std::is_move_assignable_v<OrmDatabase>);

    // Copying never was possible - SqliteConnection owns a unique handle.
    STATIC_REQUIRE(!std::is_copy_constructible_v<OrmDatabase>);
    STATIC_REQUIRE(!std::is_copy_assignable_v<OrmDatabase>);
}

TEST_CASE("OrmDatabase: createTable is deliberately unguarded", "[orm][lifetime]")
{
    // createTable is the one accessor callable on a temporary, and that is
    // sound rather than an oversight: it borrows nothing out. It execs the DDL
    // and returns void, so no reference survives pointing at a dead connection.
    // InMemory().createTable<UserTable>() is merely pointless - it builds a
    // schema in a database that dies immediately - not dangling.
    STATIC_REQUIRE(!BorrowGuarded<CanCreateTable>);

    // Spelled out, since !BorrowGuarded alone would also hold for a typo:
    STATIC_REQUIRE(CanCreateTable<OrmDatabase&>::value);
    STATIC_REQUIRE(CanCreateTable<OrmDatabase>::value);
}
} // namespace tewi::tests
