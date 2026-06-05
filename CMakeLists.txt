cmake_minimum_required(VERSION 4.0)
include(FetchContent)

project(tewi VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

# SQlite3
find_package(SQLite3 REQUIRED)

if(NOT TARGET SQLite3::SQLite3) # CMake < 4.3
  add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
endif()

# Catch2
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.15.0
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

if(TEWI_ENABLE_LOGGING)
    list(APPEND LOGGING_OPTIONS TEWI_LOG)
endif()

if(TEWI_ENABLE_TRACE)
    list(APPEND LOGGING_OPTIONS TEWI_TRACE)
endif()

add_library(${PROJECT_NAME}
        src/common/Log.h
        src/common/Logging.cppm
        src/engine/SqliteConnection.cppm
        src/engine/SqliteMigration.cppm
        src/engine/SqliteStatement.cppm
        src/engine/SqliteTransaction.cppm
)

target_sources(${PROJECT_NAME} PUBLIC
        FILE_SET CXX_MODULES FILES
        src/module.ixx
        src/common/FixedString.ixx
        src/common/Logging.ixx
        src/common/NumberTypes.ixx
        src/engine/SqliteConnection.ixx
        src/engine/SqliteError.ixx
        src/engine/SqliteMigration.ixx
        src/engine/SqliteStatement.ixx
        src/engine/SqliteTransaction.ixx
        src/orm/Column.ixx
        src/orm/Constraints.ixx
        src/orm/Index.ixx
        src/orm/JoinQuery.ixx
        src/orm/OrmDatabase.ixx
        src/orm/Projection.ixx
        src/orm/QueryRange.ixx
        src/orm/Repository.ixx
        src/orm/SelectQuery.ixx
        src/orm/Table.ixx
        src/orm/TableRegistry.ixx
        src/orm/TypeAdapter.ixx
        src/orm/detail/BaseTypeAdapters.ixx
        src/orm/detail/PrimaryKeyHelpers.ixx
        src/orm/detail/ForeignKeyHelpers.ixx
        src/orm/detail/MemberTraits.ixx
        src/orm/detail/QueryState.ixx
        src/orm/detail/TableHelpers.ixx
)

target_link_libraries(${PROJECT_NAME} PUBLIC SQLite3::SQLite3)
target_compile_definitions(${PROJECT_NAME} PUBLIC ${LOGGING_OPTIONS})
target_include_directories(${PROJECT_NAME} INTERFACE "${CMAKE_SOURCE_DIR}/include" PRIVATE "${CMAKE_SOURCE_DIR}/src")

add_subdirectory(tests)
