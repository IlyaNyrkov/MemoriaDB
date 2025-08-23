add_executable(memoriadb_tests
        row_test.cpp
        schema_test.cpp
        table_test.cpp
        database_test.cpp
        parser_test.cpp
        statement_executor_test.cpp
)

target_link_libraries(memoriadb_tests
        PRIVATE
        GTest::gtest_main
        memoriadb
)

include(GoogleTest)
gtest_discover_tests(memoriadb_tests)
