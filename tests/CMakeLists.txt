add_executable(observing_ptr_tests
    test_observing_ptr.cxx
)

target_link_libraries(observing_ptr_tests
    PRIVATE
        observing_ptr
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(observing_ptr_tests)
