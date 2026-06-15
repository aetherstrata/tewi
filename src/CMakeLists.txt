target_sources(${PROJECT_NAME} PRIVATE
        ast/AstRenderer.cppm
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
        ast/AstRenderer.ixx
        ast/AstSpec.ixx
        common/FixedString.ixx
        common/Logging.ixx
        common/NumberTypes.ixx
        engine/SqliteConnection.ixx
        engine/SqliteError.ixx
        engine/SqliteMigration.ixx
        engine/SqliteStatement.ixx
        engine/SqliteTransaction.ixx
        orm/BasicQuery.ixx
        orm/Column.ixx
        orm/Compare.ixx
        orm/Constraints.ixx
        orm/Index.ixx
        orm/Order.ixx
        orm/OrmDatabase.ixx
        orm/Repository.ixx
        orm/RowHydrator.ixx
        orm/Table.ixx
        orm/TableRegistry.ixx
        orm/TypeAdapter.ixx
        orm/detail/BaseTypeAdapters.ixx
        orm/detail/PrimaryKeyHelpers.ixx
        orm/detail/ForeignKeyHelpers.ixx
        orm/detail/MemberTraits.ixx
        orm/detail/TableHelpers.ixx
)