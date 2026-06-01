export module tewi:index;

import :fixed_string;

import std;

// ============================================================================
//  Table Indices
// ============================================================================

namespace tewi
{

// Plain index
export template <FixedString name, auto... memberPtrs>
struct Index
{
    static constexpr auto name_storage          = name;
    static constexpr std::string_view IndexName = name_storage.view();

    static constexpr bool unique = false;

    template <typename TableType>
    [[nodiscard]] static std::string create_index_sql(bool ifNotExists = true)
    {
        std::string sql = unique ? "CREATE UNIQUE INDEX " : "CREATE INDEX ";
        if (ifNotExists)
        {
            sql += "IF NOT EXISTS ";
        }
        sql += std::string(IndexName) + " ON " + std::string(TableType::TableName) + " (";

        bool first = true;
        // Fold over MemberPtrs, resolving each to its column name at compile time.
        ([&]<auto MP>()
        {
            constexpr std::string_view col = TableType::template ColumnOf<MP>::ColumnName;
            static_assert(!col.empty(),
                          "Index: member pointer not mapped to any column in this table.");
            if (!first) sql += ", ";
            sql   += std::string(col);
            first  = false;
        }.template operator()<memberPtrs>(), ...);

        sql += ");";
        return sql;
    }
};

// Unique index
export template <FixedString IndexName, auto... MemberPtrs>
struct UniqueIndex : Index<IndexName, MemberPtrs...>
{
    static constexpr bool unique = true; // shadows base
};

// Tag to delimit columns from indexes in the Table parameter list.
export template <typename... Is>
struct Indexes
{};

} // namespace tewi
