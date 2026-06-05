target_sources(${PROJECT_NAME} PRIVATE
        common/Log.h
        common/Logging.cppm
        engine/SqliteConnection.cppm
        engine/SqliteMigration.cppm
        engine/SqliteStatement.cppm
        engine/SqliteTransaction.cppm
)

target_sources(${PROJECT_NAME} PUBLIC
        FILE_SET CXX_MODULES FILES
        module.ixx
        common/FixedString.ixx
        common/Logging.ixx
        common/NumberTypes.ixx
        engine/SqliteConnection.ixx
        engine/SqliteError.ixx
        engine/SqliteMigration.ixx
        engine/SqliteStatement.ixx
        engine/SqliteTransaction.ixx
        orm/Column.ixx
        orm/Constraints.ixx
        orm/Index.ixx
        orm/JoinQuery.ixx
        orm/Order.ixx
        orm/OrmDatabase.ixx
        orm/Projection.ixx
        orm/QueryRange.ixx
        orm/Repository.ixx
        orm/SelectQuery.ixx
        orm/Table.ixx
        orm/TableRegistry.ixx
        orm/TypeAdapter.ixx
        orm/detail/BaseTypeAdapters.ixx
        orm/detail/PrimaryKeyHelpers.ixx
        orm/detail/ForeignKeyHelpers.ixx
        orm/detail/MemberTraits.ixx
        orm/detail/QueryState.ixx
        orm/detail/TableHelpers.ixx
)