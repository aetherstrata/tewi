#include <catch2/catch_test_macros.hpp>

import tewi;

namespace tewi::tests
{
// =========================================================================
//  Type Affinity Tests
// =========================================================================

TEST_CASE("Type Affinity: i32", "[orm][affinity][numerics]")
{
    SECTION("i32 shall have affinity to INTEGER")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<i32>::affinity == "INTEGER");
    }
    SECTION("i32 shall not be nullable")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<i32>::nullable == false);
    }
}

TEST_CASE("Type Affinity: i64", "[orm][affinity][numerics]")
{
    SECTION("i64 shall have affinity to INTEGER")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<i64>::affinity == "INTEGER");
    }
    SECTION("i64 shall not be nullable")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<i64>::nullable == false);
    }
}

TEST_CASE("Type Affinity: f32", "[orm][affinity][numerics]")
{
    SECTION("f32 shall have affinity to REAL")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<f32>::affinity == "REAL");
    }
    SECTION("f32 shall not be nullable")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<f32>::nullable == false);
    }
}

TEST_CASE("Type Affinity: f64", "[orm][affinity][numerics]")
{
    SECTION("f64 shall have affinity to REAL")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<f64>::affinity == "REAL");
    }
    SECTION("f64 shall not be nullable")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<f64>::nullable == false);
    }
}

TEST_CASE("Type Affinity: std::string", "[orm][affinity][text]")
{
    SECTION("std::string shall have affinity to TEXT")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<std::string>::affinity == "TEXT");
    }
    SECTION("std::string shall not be nullable")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<std::string>::nullable == false);
    }
}

TEST_CASE("Type Affinity: std::vector<std::byte>", "[orm][affinity][blob]")
{
    SECTION("std::vector<std::byte> shall have affinity to BLOB")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<std::vector<std::byte>>::affinity == "BLOB");
    }
    SECTION("std::vector<std::byte> shall not be nullable")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<std::vector<std::byte>>::nullable == false);
    }
}

TEST_CASE("Type Affinity: std::vector<u8>", "[orm][affinity][blob]")
{
    SECTION("std::vector<u8> shall have affinity to BLOB")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<std::vector<u8>>::affinity == "BLOB");
    }
    SECTION("std::vector<u8> shall not be nullable")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<std::vector<u8>>::nullable == false);
    }
}

// =========================================================================
//  Optional Type Tests
// =========================================================================

TEST_CASE("Type Affinity: std::optional<i32>", "[orm][affinity][optional]")
{
    SECTION("std::optional<i32> shall have affinity to INTEGER")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<std::optional<i32>>::affinity == "INTEGER");
    }
    SECTION("std::optional<i32> shall be nullable")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<std::optional<i32>>::nullable == true);
    }
}

TEST_CASE("Type Affinity: std::optional<i64>", "[orm][affinity][optional]")
{
    SECTION("std::optional<i64> shall have affinity to INTEGER")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<std::optional<i64>>::affinity == "INTEGER");
    }
    SECTION("std::optional<i64> shall be nullable")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<std::optional<i64>>::nullable == true);
    }
}

TEST_CASE("Type Affinity: std::optional<f32>", "[orm][affinity][optional]")
{
    SECTION("std::optional<f32> shall have affinity to REAL")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<std::optional<f32>>::affinity == "REAL");
    }
    SECTION("std::optional<f32> shall be nullable")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<std::optional<f32>>::nullable == true);
    }
}

TEST_CASE("Type Affinity: std::optional<f64>", "[orm][affinity][optional]")
{
    SECTION("std::optional<f64> shall have affinity to REAL")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<std::optional<f64>>::affinity == "REAL");
    }
    SECTION("std::optional<f64> shall be nullable")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<std::optional<f64>>::nullable == true);
    }
}

TEST_CASE("Type Affinity: std::optional<std::string>", "[orm][affinity][optional]")
{
    SECTION("std::optional<std::string> shall have affinity to TEXT")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<std::optional<std::string>>::affinity == "TEXT");
    }
    SECTION("std::optional<std::string> shall be nullable")
    {
        STATIC_REQUIRE(SqliteTypeAdapter<std::optional<std::string>>::nullable == true);
    }
}
} // namespace tewi::tests
