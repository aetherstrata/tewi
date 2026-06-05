include(FetchContent)

# Catch2
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.15.0
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

add_executable(${PROJECT_NAME}_tests
        ColumnSchemaTests.cpp
        PrimaryKeyTests.cpp
        OrmTests.cpp
        LogInitializer.cpp
        TypeAffinityTests.cpp
        TableRegistryTests.cpp
        TableMetaTests.cpp
        CreateTableTests.cpp
        Tables.h
)

target_link_libraries(${PROJECT_NAME}_tests PRIVATE Catch2::Catch2WithMain ${PROJECT_NAME})
target_compile_definitions(${PROJECT_NAME}_tests PRIVATE ${LOGGING_OPTIONS})

include(CTest)
include(Catch)
catch_discover_tests(${PROJECT_NAME}_tests)
