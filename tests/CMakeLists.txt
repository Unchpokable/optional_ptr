add_executable(optional_ptr_tests
    test_optional_ptr.cxx
)

target_link_libraries(optional_ptr_tests
    PRIVATE
        optional_ptr
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(optional_ptr_tests)
