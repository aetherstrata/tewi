add_executable(${PROJECT_NAME}_tests
        ColumnSchemaTests.cpp
        OrmTests.cpp
        TableSchemaTests.cpp
)

target_link_libraries(${PROJECT_NAME}_tests PRIVATE Catch2::Catch2WithMain ${PROJECT_NAME})
target_compile_definitions(${PROJECT_NAME}_tests PRIVATE ${LOGGING_OPTIONS})

include(CTest)
include(Catch)
catch_discover_tests(${PROJECT_NAME}_tests)
